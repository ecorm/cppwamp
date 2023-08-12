/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPLISTENER_HPP
#define CPPWAMP_INTERNAL_HTTPLISTENER_HPP

#include <memory>
#include <set>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include "../asiodefs.hpp"
#include "../codec.hpp"
#include "../version.hpp"
#include "../transports/httpendpoint.hpp"
#include "httptransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpListener : public std::enable_shared_from_this<HttpListener>
{
public:
    using Ptr       = std::shared_ptr<HttpListener>;
    using Settings  = HttpEndpoint;
    using CodecIds  = std::set<int>;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using Socket    = HttpTransport::Socket;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = HttpTransport;

    static Ptr create(IoStrand i, Settings s, CodecIds codecIds)
    {
        return Ptr(new HttpListener(std::move(i), std::move(s),
                                         std::move(codecIds)));
    }

    void establish(Handler&& handler)
    {
        assert(!handler_ &&
               "HttpListener establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        acceptor_.async_accept(
            tcpSocket_,
            [this, self](boost::system::error_code bec)
            {
                if (check(bec))
                    receiveUpgrade();
            });
    }

    void cancel()
    {
        if (http_)
            http_->next_layer().close();
        else if (tcpSocket_.is_open())
            tcpSocket_.close();
        else
            acceptor_.cancel();
    }

private:
    using TcpSocket = boost::asio::ip::tcp::socket;
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

    HttpListener(IoStrand i, Settings s, CodecIds codecIds)
        : strand_(std::move(i)),
          settings_(std::move(s)),
          acceptor_(strand_, makeEndpoint(settings_)),
          codecIds_(std::move(codecIds)),
          noSubprotocolResponse_(boost::beast::http::status::bad_request,
                                 11, "No subprotocol was provided"),
          badSubprotocolResponse_(boost::beast::http::status::bad_request,
                                  11, "The given subprotocol is not supported"),
          tcpSocket_(strand_)
    {
        static constexpr auto serverField = boost::beast::http::field::server;
        noSubprotocolResponse_.set(serverField, Version::agentString());
        noSubprotocolResponse_.prepare_payload();
        badSubprotocolResponse_.set(serverField, Version::agentString());
        badSubprotocolResponse_.prepare_payload();
    }

    void receiveUpgrade()
    {
        settings_.options().applyTo(tcpSocket_);

        // http::stream does not provide a means to inspect request
        // headers, so use the workaround suggested here:
        // https://www.boost.org/doc/libs/release/libs/beast/doc/html/beast/using_http/handshaking.html#beast.using_http.handshaking.inspecting_http_requests
        auto self = shared_from_this();
        boost::beast::http::async_read(
            tcpSocket_, buffer_, upgrade_,
            [this, self] (const boost::beast::error_code& bec, std::size_t)
            {
                if (check(bec))
                    acceptHandshake();
            });
    }

    void acceptHandshake()
    {
        // TODO: Multiplex http transports with same port but different
        //       request-target URIs.

        // Check that we actually received a http upgrade request
        if (!boost::beast::websocket::is_upgrade(upgrade_))
            return fail(boost::beast::websocket::error::no_connection_upgrade);

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

        // Transfer the TCP socket to a new http stream
        http_ = SocketPtr{new Socket(std::move(tcpSocket_))};

        // Set the Server field of the handshake
        using boost::beast::http::field;
        setHttpHandshakeField(field::server, Version::agentString());

        // Set the Sec-http-Protocol field of the handshake
        setHttpHandshakeField(field::sec_websocket_protocol, subprotocol);

        // Complete the handshake
        auto self = shared_from_this();
        http_->async_accept(upgrade_,
            [this, self, codecId](boost::beast::error_code bec)
            {
                if (check(bec))
                    complete(codecId);
            });
    }

    void respondThenFail(const Response& response, TransportErrc errc)
    {
        namespace http = boost::beast::http;
        auto self = shared_from_this();
        http::async_write(
            tcpSocket_, response,
            [this, self, errc](boost::beast::error_code bec, std::size_t)
            {
                fail(errc);
            });
    }

    template <typename T>
    void setHttpHandshakeField(boost::beast::http::field field, T&& value)
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

        http_->set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{std::forward<T>(value), field}));
    }

    void complete(int codecId)
    {
        if (subprotocolIsText(codecId))
            http_->text(true);
        else
            http_->binary(true);

        http_->read_message_max(settings_.maxRxLength());

        const TransportInfo i{codecId,
                              std::numeric_limits<std::size_t>::max(),
                              settings_.maxRxLength()};
        Transporting::Ptr transport{Transport::create(std::move(http_),
                                                      i)};
        http_.reset();
        dispatchHandler(std::move(transport));
    }

    bool check(boost::beast::error_code bec)
    {
        if (bec)
        {
            http_.reset();
            tcpSocket_.close();
            auto ec = static_cast<std::error_code>(bec);
            if (bec == std::errc::operation_canceled ||
                bec == boost::asio::error::operation_aborted)
            {
                ec = make_error_code(TransportErrc::aborted);
            }
            dispatchHandler(UnexpectedError(ec));
        }
        return !bec;
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        http_.reset();
        tcpSocket_.close();
        dispatchHandler(makeUnexpectedError(errc));
    }

    template <typename TArg>
    void dispatchHandler(TArg&& arg)
    {
        const Handler handler(std::move(handler_));
        handler_ = nullptr;
        handler(std::forward<TArg>(arg));
    }

    IoStrand strand_;
    Settings settings_;
    boost::asio::ip::tcp::acceptor acceptor_;
    CodecIds codecIds_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    Response noSubprotocolResponse_;
    Response badSubprotocolResponse_;
    boost::beast::http::request<boost::beast::http::string_body> upgrade_;
    TcpSocket tcpSocket_;
    SocketPtr http_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPLISTENER_HPP
