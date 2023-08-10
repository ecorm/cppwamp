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
#include "../version.hpp"
#include "../transports/websocketendpoint.hpp"
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
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using Socket    = WebsocketTransport::Socket;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = WebsocketTransport;

    static Ptr create(IoStrand i, Settings s, CodecIds codecIds)
    {
        return Ptr(new WebsocketListener(std::move(i), std::move(s),
                                         std::move(codecIds)));
    }

    void establish(Handler&& handler)
    {
        assert(!handler_ &&
               "WebsocketListener establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        acceptor_.async_accept(
            tcpSocket_,
            [this, self](boost::system::error_code asioEc)
            {
                if (check(asioEc))
                    receiveUpgrade();
            });
    }

    void cancel()
    {
        if (websocket_)
            websocket_->close(boost::beast::websocket::going_away);
        else if (tcpSocket_.is_open())
            tcpSocket_.close();
        else
            acceptor_.cancel();
    }

private:
    using TcpSocket = boost::asio::ip::tcp::socket;

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
            return KnownCodecIds::msgpack();
        return 0;
    }

    WebsocketListener(IoStrand i, Settings s, CodecIds codecIds)
        : strand_(std::move(i)),
          acceptor_(i),
          settings_(std::move(s)),
          codecIds_(std::move(codecIds)),
          tcpSocket_(strand_)
    {}

    void receiveUpgrade()
    {
        settings_.options().applyTo(tcpSocket_);

        // websocket::stream does not provide a means to inspect request
        // headers, so use the workaround suggested here:
        // https://github.com/boostorg/beast/issues/2549
        auto self = shared_from_this();
        boost::beast::http::async_read(
            tcpSocket_, buffer_, upgrade_,
            [this, self] (const boost::beast::error_code& asioEc, std::size_t)
            {
                if (check(asioEc))
                    acceptHandshake();
            });
    }

    void acceptHandshake()
    {
        // Check that we actually received a websocket upgrade request
        if (!boost::beast::websocket::is_upgrade(upgrade_))
            return fail(boost::beast::websocket::error::no_connection_upgrade);

        // Parse the websocket protocol to determine the peer's desired codec
        auto found = upgrade_.base().find("Sec-WebSocket-Protocol");
        if (found == upgrade_.base().end())
            return fail(TransportErrc::noSerializer);
        auto codecId = parseSubprotocol(found->value());
        if (codecIds_.count(codecId) == 0)
            return fail(TransportErrc::badSerializer);

        // Transfer the TCP socket to a new websocket stream
        websocket_ = SocketPtr{new Socket(std::move(tcpSocket_))};

        // Set the Server field of the handshake
        using boost::beast::http::field;
        setWebsocketHandshakeField(field::server, Version::agentString());

        // Complete the handshake
        auto self = shared_from_this();
        websocket_->async_accept(upgrade_,
            [this, self, codecId](boost::beast::error_code asioEc)
            {
                if (check(asioEc))
                    complete(codecId);
            });
    }

    template <typename T>
    void setWebsocketHandshakeField(boost::beast::http::field field, T&& value)
    {
        namespace websocket = boost::beast::websocket;

        struct Decorator
        {
            ValueTypeOf<T> value;
            boost::beast::http::field field;

            void operator()(websocket::request_type& req)
            {
                req.set(field, std::move(value));
            }
        };

        websocket_->set_option(websocket::stream_base::decorator(
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

    bool check(boost::beast::error_code asioEc)
    {
        if (asioEc)
        {
            websocket_.reset();
            tcpSocket_.close();
            auto ec = static_cast<std::error_code>(asioEc);
            if (asioEc == std::errc::operation_canceled ||
                asioEc == boost::asio::error::operation_aborted)
            {
                ec = make_error_code(TransportErrc::aborted);
            }
            dispatchHandler(UnexpectedError(ec));
        }
        return !asioEc;
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        websocket_.reset();
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
    boost::asio::ip::tcp::acceptor acceptor_;
    Settings settings_;
    CodecIds codecIds_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> upgrade_;
    TcpSocket tcpSocket_;
    SocketPtr websocket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
