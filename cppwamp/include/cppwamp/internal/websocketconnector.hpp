/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP

#include <array>
#include <limits>
#include <string>
#include <memory>
#include <boost/asio/connect.hpp>
#include "../asiodefs.hpp"
#include "../version.hpp"
#include "../transports/httpprotocol.hpp"
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
    using Socket    = WebsocketTransport::WebsocketSocket;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = WebsocketClientTransport;

    static Ptr create(IoStrand i, Settings s, int codecId)
    {
        return Ptr(new WebsocketConnector(std::move(i), std::move(s), codecId));
    }

    void establish(Handler handler)
    {
        assert(!handler_ &&
               "WebsocketConnector establishment already in progress");
        handler_ = std::move(handler);
        auto self = shared_from_this();
        resolver_.async_resolve(
            settings_.hostName(), settings_.serviceName(),
            [this, self](boost::beast::error_code netEc,
                         tcp::resolver::results_type endpoints)
            {
                if (check(netEc))
                    connect(endpoints);
            });
    }

    void cancel()
    {
        if (websocket_)
            websocket_->next_layer().close();
        else
            resolver_.cancel();
    }

private:
    using tcp = boost::asio::ip::tcp;

    static const std::string& subprotocolString(int codecId)
    {
        static const std::array<std::string, KnownCodecIds::count() + 1> ids =
        {
            "",
            "wamp.2.json",
            "wamp.2.msgpack",
            "wamp.2.cbor"
        };

        if (codecId > KnownCodecIds::count())
            return ids[0];
        return ids.at(codecId);
    }

    static bool subprotocolIsText(int codecId)
    {
        return codecId == KnownCodecIds::json();
    }

    WebsocketConnector(IoStrand i, Settings s, int codecId)
        : strand_(std::move(i)),
          settings_(std::move(s)),
          resolver_(strand_),
          codecId_(codecId)
    {}

    void connect(const tcp::resolver::results_type& endpoints)
    {
        assert(!websocket_);
        websocket_ = SocketPtr{new Socket(strand_)};
        auto& tcpSocket = websocket_->next_layer();
        tcpSocket.open(boost::asio::ip::tcp::v4());
        settings_.socketOptions().applyTo(tcpSocket);

        auto self = shared_from_this();
        boost::asio::async_connect(
            tcpSocket, endpoints,
            [this, self](boost::beast::error_code netEc,
                         const tcp::endpoint& ep)
            {
                if (check(netEc))
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

        // Set the User-Agent field of the handshake
        using boost::beast::http::field;
        setWebsocketHandshakeField(field::user_agent, Version::agentString());

        // Set the Sec-WebSocket-Protocol field of the handshake to match
        // the desired codec
        const auto& subprotocol = subprotocolString(codecId_);
        assert(!subprotocol.empty());
        setWebsocketHandshakeField(field::sec_websocket_protocol, subprotocol);

        auto self = shared_from_this();
        websocket_->async_handshake(
            response_, host, settings_.target(),
            [this, self](boost::beast::error_code netEc)
            {
                auto status = static_cast<HttpStatus>(response_.result());
                response_.clear();
                response_.body().clear();

                if (netEc == boost::beast::websocket::error::upgrade_declined)
                    return fail(make_error_code(status));
                if (check(netEc))
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

        websocket_->set_option(websocket::stream_base::decorator(
            Decorator{std::forward<T>(value), field}));
    }

    void complete()
    {
        if (subprotocolIsText(codecId_))
            websocket_->text(true);
        else
            websocket_->binary(true);

        websocket_->read_message_max(settings_.maxRxLength());

        const TransportInfo i{codecId_,
                              std::numeric_limits<std::size_t>::max(),
                              settings_.maxRxLength(),
                              settings_.heartbeatInterval()};
        Transporting::Ptr transport{Transport::create(std::move(websocket_),
                                                      i)};
        websocket_.reset();
        dispatchHandler(std::move(transport));
    }

    bool check(boost::beast::error_code netEc)
    {
        if (netEc)
        {
            auto ec = static_cast<std::error_code>(netEc);
            if (netEc == boost::asio::error::operation_aborted)
                ec = make_error_code(TransportErrc::aborted);
            else if (netEc == boost::beast::websocket::error::upgrade_declined)
                ec = make_error_code(TransportErrc::handshakeDeclined);
            fail(ec);
        }
        return !netEc;
    }

    void fail(std::error_code ec)
    {
        websocket_.reset();
        dispatchHandler(makeUnexpected(ec));
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
    SocketPtr websocket_;
    boost::beast::websocket::response_type response_;
    Handler handler_;
    int codecId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP
