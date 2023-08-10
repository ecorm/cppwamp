/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP

#include <limits>
#include <memory>
#include <boost/asio/connect.hpp>
#include "../asiodefs.hpp"
#include "../version.hpp"
#include "../transports/websockethost.hpp"
#include "../traits.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class WebsocketConnector
    : public std::enable_shared_from_this<WebsocketConnector>
{
public:
    using Ptr       = std::shared_ptr<WebsocketConnector>;
    using Settings  = WebsocketHost;
    using Socket    = WebsocketTransport::Socket;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = WebsocketTransport;

    static Ptr create(IoStrand i, Settings s, int codecId)
    {
        return Ptr(new WebsocketConnector(std::move(i), std::move(s), codecId));
    }

    void establish(Handler&& handler)
    {
        assert(!handler_ &&
               "WebsocketConnector establishment already in progress");
        handler_ = std::move(handler);
        auto self = shared_from_this();
        resolver_.async_resolve(
            settings_.hostName(), settings_.serviceName(),
            [this, self](boost::system::error_code asioEc,
                         tcp::resolver::results_type endpoints)
            {
                if (check(asioEc))
                    connect(endpoints);
            });
    }

    void cancel()
    {
        if (socket_)
            socket_->close(boost::beast::websocket::going_away);
        else
            ;// TODO: Cancel resolver
    }

private:
    using tcp = boost::asio::ip::tcp;

    WebsocketConnector(IoStrand i, Settings s, int codecId)
        : strand_(std::move(i)),
          settings_(std::move(s)),
          resolver_(strand_),
          codecId_(codecId)
    {}

    void connect(const tcp::resolver::results_type& endpoints)
    {
        assert(!socket_);
        socket_ = SocketPtr{new Socket(strand_)};
        auto& tcpSocket = socket_->next_layer();
        tcpSocket.open(boost::asio::ip::tcp::v4());
        settings_.options().applyTo(tcpSocket);

        auto self = shared_from_this();
        boost::asio::async_connect(
            tcpSocket, endpoints,
            [this, self](boost::system::error_code asioEc,
                         const tcp::endpoint& ep)
            {
                if (check(asioEc))
                    handshake(ep);
            });
    }

    void handshake(const tcp::endpoint& ep)
    {
        /*  Update the host string. This will provide the value of the
            host HTTP header during the WebSocket handshake.
            See https://tools.ietf.org/html/rfc7230#section-5.4 */
        std::string host = settings_.hostName() + ':' +
                           std::to_string(ep.port());

        namespace websocket = boost::beast::websocket;
        namespace http = boost::beast::http;

        // Set the User-Agent field of the handshake
        setWebsocketHandshakeField(http::field::user_agent,
                                   Version::agentString());

        // Set the Sec-WebSocket-Protocol field of the handshake to match
        // the desired codec
        const auto& subprotocol = websocketSubprotocolString(codecId_);
        assert(!subprotocol.empty());
        setWebsocketHandshakeField(http::field::sec_websocket_protocol,
                                   subprotocol);

        auto self = shared_from_this();
        socket_->async_handshake(
            host, "/",
            [this, self](boost::system::error_code asioEc)
            {
                if (check(asioEc))
                    complete();
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

        socket_->set_option(websocket::stream_base::decorator(
            Decorator{std::forward<T>(value), field}));
    }

    void complete()
    {
        if (websocketSubprotocolIsText(codecId_))
            socket_->text(true);
        else
            socket_->binary(true);

        socket_->read_message_max(settings_.maxRxLength());

        const TransportInfo i{codecId_,
                              std::numeric_limits<std::size_t>::max(),
                              settings_.maxRxLength(),
                              settings_.heartbeatInterval()};
        Transporting::Ptr transport{Transport::create(std::move(socket_), i)};
        socket_.reset();
        dispatchHandler(std::move(transport));
    }

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            socket_.reset();
            auto ec = static_cast<std::error_code>(asioEc);
            if (asioEc == boost::asio::error::operation_aborted)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(makeUnexpected(ec));
        }
        return !asioEc;
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
    boost::asio::ip::tcp::resolver resolver_;
    SocketPtr socket_;
    Handler handler_;
    int codecId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP
