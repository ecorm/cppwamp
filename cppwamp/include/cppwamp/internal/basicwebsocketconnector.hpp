/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_BASICWEBSOCKETCONNECTOR_HPP
#define CPPWAMP_INTERNAL_BASICWEBSOCKETCONNECTOR_HPP

#include <array>
#include <string>
#include <memory>
#include <boost/asio/connect.hpp>
#include <boost/optional/optional.hpp>
#include "../asiodefs.hpp"
#include "../transports/httpstatus.hpp"
#include "basicwebsockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TTraits>
class BasicWebsocketConnector
    : public std::enable_shared_from_this<BasicWebsocketConnector<TTraits>>
{
public:
    using Traits    = TTraits;
    using Ptr       = std::shared_ptr<BasicWebsocketConnector>;
    using Settings  = typename Traits::ClientSettings;
    using Socket    = typename Traits::Socket;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using Transport = BasicWebsocketClientTransport<Traits>;

    BasicWebsocketConnector(IoStrand i, Settings s, int codecId)
        : strand_(std::move(i)),
          settings_(std::make_shared<Settings>(std::move(s))),
          resolver_(strand_),
          codecId_(codecId)
    {}

    void establish(Handler handler)
    {
        assert(!handler_ &&
               "WebsocketConnector establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        resolver_.async_resolve(
            settings_->address(), settings_->serviceName(),
            [this, self](boost::beast::error_code netEc,
                         tcp::resolver::results_type endpoints)
            {
                if (check(netEc))
                    connect(IsTls{}, endpoints);
            });
    }

    void cancel()
    {
        if (websocket_.has_value())
            tcpLayer().close();
        else
            resolver_.cancel();
    }

private:
    using tcp            = boost::asio::ip::tcp;
    using TcpSocket      = tcp::socket;
    using IsTls          = typename Traits::IsTls;
    using SslContextType = typename Traits::SslContextType;

    struct Decorator
    {
        std::string agent;
        std::string subprotocol;

        void operator()(boost::beast::websocket::request_type& req)
        {
            using boost::beast::http::field;
            req.set(field::user_agent, agent);
            req.set(field::sec_websocket_protocol, subprotocol);
        }
    };

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

    // Non-TLS overload
    template <typename E>
    void connect(FalseType, const E& resolverEndpoints)
    {
        assert(!websocket_.has_value());
        websocket_.emplace(strand_);

        auto& tcpSocket = tcpLayer();
        tcpSocket.open(tcp::v4());
        settings_->socketOptions().applyTo(tcpSocket);

        auto self = this->shared_from_this();
        boost::asio::async_connect(
            tcpSocket, resolverEndpoints,
            [this, self](boost::beast::error_code netEc,
                         const tcp::endpoint& ep)
            {
                if (check(netEc))
                    tlsHandshake(FalseType{}, ep);
            });
    }

    // TLS overload
    template <typename E>
    void connect(TrueType, const E& resolverEndpoints)
    {
        auto contextOrError = settings_->makeSslContext({});
        if (!contextOrError.has_value())
            return fail(contextOrError.error());
        sslContext_ = std::move(contextOrError).value();

        assert(!websocket_.has_value());
        websocket_.emplace(strand_, sslContext_.get());

        auto ec = Traits::initializeClientSocket(*websocket_, *settings_);
        if (ec)
            return fail(ec);

        auto& tcpSocket = tcpLayer();
        tcpSocket.open(tcp::v4());
        settings_->socketOptions().applyTo(tcpSocket);

        auto self = this->shared_from_this();
        boost::asio::async_connect(
            tcpSocket, resolverEndpoints,
            [this, self](boost::beast::error_code netEc,
                         const tcp::endpoint& ep)
            {
                if (check(netEc))
                    tlsHandshake(TrueType{}, ep);
            });
    }

    // Non-TLS overload
    template <typename E>
    void tlsHandshake(FalseType, const E& connectedEndpoint)
    {
        websocketHandshake(connectedEndpoint);
    }

    // TLS overload
    template <typename E>
    void tlsHandshake(TrueType, const E& connectedEndpoint)
    {
        auto self = this->shared_from_this();
        Traits::sslClientHandshake(
            *websocket_,
            [this, self, connectedEndpoint](boost::system::error_code netEc)
            {
                if (netEc)
                    return fail(netEc);
                websocketHandshake(connectedEndpoint);
            });
    }

    void websocketHandshake(const tcp::endpoint& ep)
    {
        /*  Update the host string. This will provide the value of the
            host HTTP header during the WebSocket handshake.
            See https://tools.ietf.org/html/rfc7230#section-5.4 */
        std::string host = settings_->address() + ':' +
                           std::to_string(ep.port());

        // Set the User-Agent and Sec-WebSocket-Protocol fields of the
        // upgrade request
        const auto& subprotocol = subprotocolString(codecId_);
        assert(!subprotocol.empty());
        assert(websocket_.has_value());
        websocket_->set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{settings_->options().agent(), subprotocol}));

        setWebsocketOptions(*websocket_, *settings_, false);

        // Perform the handshake
        auto self = this->shared_from_this();
        websocket_->async_handshake(
            response_, host, settings_->target(),
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

    void complete()
    {
        assert(websocket_.has_value());

        if (subprotocolIsText(codecId_))
            websocket_->text(true);
        else
            websocket_->binary(true);

        TransportInfo info{codecId_, settings_->limits().wampWriteMsgSize(),
                           settings_->limits().wampReadMsgSize()};
        Transporting::Ptr transport = std::make_shared<Transport>(
            std::move(*websocket_), settings_, std::move(info),
            std::move(sslContext_));
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
        sslContext_ = {};
        dispatchHandler(makeUnexpected(ec));
    }

    template <typename TArg>
    void dispatchHandler(TArg&& arg)
    {
        const Handler handler(std::move(handler_));
        handler_ = nullptr;
        handler(std::forward<TArg>(arg));
    }

    TcpSocket& tcpLayer() {return Traits::tcpLayer(*websocket_);}

    IoStrand strand_;
    std::shared_ptr<Settings> settings_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::optional<Socket> websocket_;
    boost::beast::websocket::response_type response_;
    Handler handler_;
    SslContextType sslContext_;
    int codecId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BASICWEBSOCKETCONNECTOR_HPP
