/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP

#include <memory>
#include <set>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include "../asiodefs.hpp"
#include "../codec.hpp"
#include "../listener.hpp"
#include "../version.hpp"
#include "../transports/websocketendpoint.hpp"
#include "tcpacceptor.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class WebsocketListener : public std::enable_shared_from_this<WebsocketListener>
{
public:
    using Ptr       = std::shared_ptr<WebsocketListener>;
    using Settings  = WebsocketEndpoint;
    using CodecIds  = std::set<int>;
    using Handler   = Listening::Handler;

    static Ptr create(AnyIoExecutor e, IoStrand i, Settings s,
                      CodecIds codecIds)
    {
        return Ptr(new WebsocketListener(std::move(e), std::move(i),
                                         std::move(s), std::move(codecIds)));
    }

    void observe(Handler handler) {handler_ = std::move(handler);}

    void establish()
    {
        assert(!establishing_ && "WebsocketListener already establishing");
        establishing_ = true;
        auto self = this->shared_from_this();
        acceptor_.establish(
            [this, self](AcceptorResult result)
            {
                if (checkAcceptorError(result))
                {
                    tcpSocket_ = std::move(result.socket);
                    receiveUpgrade();
                }
            });
    }

    void cancel()
    {
        if (websocket_)
            websocket_->next_layer().close();
        else if (tcpSocket_)
            tcpSocket_->close();
        else
            acceptor_.cancel();
    }

private:
    using AcceptorConfig = BasicTcpAcceptorConfig<WebsocketEndpoint>;
    using Acceptor       = RawsockAcceptor<AcceptorConfig>;
    using AcceptorResult = typename Acceptor::Result;
    using Socket         = WebsocketTransport::Socket;
    using SocketPtr      = std::unique_ptr<Socket>;
    using Transport      = WebsocketTransport;
    using TcpSocket      = boost::asio::ip::tcp::socket;
    using Response =
        boost::beast::http::response<boost::beast::http::string_body>;

    static boost::asio::ip::tcp::endpoint makeEndpoint(const Settings& s)
    {
        if (s.address().empty())
            return {boost::asio::ip::tcp::v4(), s.port()};
        return {boost::asio::ip::make_address(s.address()), s.port()};
    }

    static bool subprotocolIsText(int codecId)
    {
        return codecId == KnownCodecIds::json();
    }

    static int parseSubprotocol(boost::beast::string_view field)
    {
        if (field == "wamp.2.json")
            return KnownCodecIds::json();
        else if (field == "wamp.2.msgpack")
            return KnownCodecIds::msgpack();
        else if (field == "wamp.2.cbor")
            return KnownCodecIds::cbor();
        return 0;
    }

    static std::error_code convertNetError(boost::system::error_code netEc)
    {
        auto ec = static_cast<std::error_code>(netEc);
        if (netEc == std::errc::operation_canceled)
        {
            ec = make_error_code(TransportErrc::aborted);
        }
        else if (netEc == std::errc::connection_reset ||
                 netEc == boost::asio::error::eof)
        {
            ec = make_error_code(TransportErrc::disconnected);
        }
        return ec;
    }

    WebsocketListener(AnyIoExecutor e, IoStrand i, Settings s,
                      CodecIds codecIds)
        : settings_(std::move(s)),
          codecIds_(std::move(codecIds)),
          acceptor_(std::move(e), std::move(i), settings_),
          buffer_(settings_.maxRxLength()),
          noSubprotocolResponse_(boost::beast::http::status::bad_request,
                                 11, "No subprotocol was provided"),
          badSubprotocolResponse_(boost::beast::http::status::bad_request,
                                  11, "The given subprotocol is not supported")
    {
        static constexpr auto serverField = boost::beast::http::field::server;
        noSubprotocolResponse_.set(serverField, Version::agentString());
        noSubprotocolResponse_.prepare_payload();
        badSubprotocolResponse_.set(serverField, Version::agentString());
        badSubprotocolResponse_.prepare_payload();
    }

    void receiveUpgrade()
    {
        settings_.options().applyTo(*tcpSocket_);

        // websocket::stream does not provide a means to inspect request
        // headers, so use the workaround suggested here:
        // https://www.boost.org/doc/libs/release/libs/beast/doc/html/beast/using_websocket/handshaking.html#beast.using_websocket.handshaking.inspecting_http_requests
        auto self = shared_from_this();
        boost::beast::http::async_read(
            *tcpSocket_, buffer_, upgrade_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (checkReadError(netEc))
                    acceptHandshake();
            });
    }

    void acceptHandshake()
    {
        // TODO: Multiplex websocket transports with same port but different
        //       request-target URIs.

        // Check that we actually received a websocket upgrade request
        if (!boost::beast::websocket::is_upgrade(upgrade_))
        {
            return fail(boost::beast::websocket::error::no_connection_upgrade,
                        ListeningErrorCategory::transient,
                        "websocket receive handshake");
        }

        // Parse the subprotocol to determine the peer's desired codec
        using boost::beast::http::field;
        auto found = upgrade_.base().find(field::sec_websocket_protocol);
        if (found == upgrade_.base().end())
        {
            return respondThenFail(noSubprotocolResponse_,
                                   TransportErrc::noSerializer);
        }
        auto subprotocol = found->value();
        auto codecId = parseSubprotocol(subprotocol);
        if (codecIds_.count(codecId) == 0)
        {
            return respondThenFail(badSubprotocolResponse_,
                                   TransportErrc::badSerializer);
        }

        // Transfer the TCP socket to a new websocket stream
        websocket_ = SocketPtr{new Socket(std::move(*tcpSocket_))};
        tcpSocket_.reset();

        // Set the Server field of the handshake
        using boost::beast::http::field;
        setWebsocketHandshakeField(field::server, Version::agentString());

        // Set the Sec-WebSocket-Protocol field of the handshake
        setWebsocketHandshakeField(field::sec_websocket_protocol, subprotocol);

        // Complete the handshake
        auto self = shared_from_this();
        websocket_->async_accept(upgrade_,
            [this, self, codecId](boost::beast::error_code netEc)
            {
                if (checkUpgrade(netEc))
                    complete(codecId);
            });
    }

    void respondThenFail(const Response& response, TransportErrc errc)
    {
        namespace http = boost::beast::http;
        auto self = shared_from_this();
        http::async_write(
            *tcpSocket_, response,
            [this, self, errc](boost::beast::error_code netEc, std::size_t)
            {
                if (checkWriteError(netEc))
                {
                    fail(errc, ListeningErrorCategory::transient,
                         "websocket receive handshake");
                }
            });
    }

    template <typename T>
    void setWebsocketHandshakeField(boost::beast::http::field field, T&& value)
    {
        namespace http = boost::beast::http;

        struct Decorator
        {
            ValueTypeOf<T> value;
            http::field field;

            void operator()(http::response_header<>& header)
            {
                header.set(field, std::move(value));
            }
        };

        websocket_->set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{std::forward<T>(value), field}));
    }

    void complete(int codecId)
    {
        if (subprotocolIsText(codecId))
            websocket_->text(true);
        else
            websocket_->binary(true);

        websocket_->read_message_max(settings_.maxRxLength());

        const TransportInfo i{codecId,
                              std::numeric_limits<std::size_t>::max(),
                              settings_.maxRxLength()};
        Transporting::Ptr transport{Transport::create(std::move(websocket_),
                                                      i)};
        websocket_.reset();
        dispatchHandler(std::move(transport));
    }

    bool checkAcceptorError(const AcceptorResult& result)
    {
        if (result.socket != nullptr)
            return true;
        fail(result.error, result.category, result.operation);
        return false;
    }

    bool checkReadError(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        auto cat = SocketErrorHelper::isReceiveFatalError(netEc)
                       ? ListeningErrorCategory::transient
                       : ListeningErrorCategory::fatal;
        fail(convertNetError(netEc), cat, "socket recv");
        return false;
    }

    bool checkUpgrade(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        fail(convertNetError(netEc), ListeningErrorCategory::transient,
             "websocket send handshake");
        return false;
    }

    bool checkWriteError(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        auto cat = SocketErrorHelper::isSendFatalError(netEc)
                       ? ListeningErrorCategory::transient
                       : ListeningErrorCategory::fatal;
        fail(convertNetError(netEc), cat, "socket send");
        return false;
    }

    template <typename TErrc>
    void fail(TErrc errc, ListeningErrorCategory cat, const char* op)
    {
        fail(make_error_code(errc), cat, op);
    }

    void fail(boost::system::error_code netEc, ListeningErrorCategory cat,
              const char* op)
    {
        fail(static_cast<std::error_code>(netEc), cat, op);
    }

    void fail(std::error_code ec, ListeningErrorCategory cat, const char* op)
    {
        websocket_.reset();
        tcpSocket_.reset();
        dispatchHandler({ec, cat, op});
    }

    void dispatchHandler(ListenResult result)
    {
        establishing_ = false;
        if (handler_)
            handler_(std::move(result));
    }

    Settings settings_;
    CodecIds codecIds_;
    Acceptor acceptor_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    Response noSubprotocolResponse_;
    Response badSubprotocolResponse_;
    boost::beast::http::request<boost::beast::http::string_body> upgrade_;
    std::unique_ptr<TcpSocket> tcpSocket_;
    SocketPtr websocket_;
    bool establishing_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
