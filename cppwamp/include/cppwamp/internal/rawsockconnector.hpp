/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKCONNECTOR_HPP
#define CPPWAMP_INTERNAL_RAWSOCKCONNECTOR_HPP

#include <cassert>
#include <memory>
#include <utility>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../traits.hpp"
#include "../transport.hpp"
#include "rawsockhandshake.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TResolver>
class RawsockConnector
    : public std::enable_shared_from_this<RawsockConnector<TResolver>>
{
public:
    using Resolver = TResolver;
    using Ptr      = std::shared_ptr<RawsockConnector>;
    using Settings = typename Resolver::Settings;
    using Handler  = std::function<void (ErrorOr<Transporting::Ptr>)>;

    RawsockConnector(IoStrand i, Settings s, int codecId)
        : resolver_(i),
          sslContext_(Traits::makeClientSslContext(s)),
          socket_(Traits::makeClientSocket(std::move(i), s, sslContext_)),
          settings_(std::make_shared<Settings>(std::move(s))),
          codecId_(codecId)
    {}

    void establish(Handler handler)
    {
        assert(!handler_ &&
               "RawsockConnector establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        resolver_.resolve(
            *settings_,
            [this, self](boost::system::error_code netEc,
                         ResolverResult endpoints)
            {
                if (check(netEc))
                    connect(endpoints);
            }
        );
    }

    void cancel()
    {
        resolver_.cancel();
        underlyingSocket().close();
    }

private:
    using Handshake         = internal::RawsockHandshake;
    using Traits            = typename Resolver::Traits;
    using Transport         = typename Resolver::Transport;
    using ResolverResult    = typename Resolver::Result;
    using NetProtocol       = typename Traits::NetProtocol;
    using Socket            = typename Traits::Socket;
    using UnderlyingSocket  = typename Traits::UnderlyingSocket;
    using IsTls             = typename Traits::IsTls;
    using SslContextType    = typename Traits::SslContextType;

    void connect(const ResolverResult& endpoints)
    {
        assert(!underlyingSocket().is_open());
        settings_->socketOptions().applyTo(underlyingSocket());

        auto self = this->shared_from_this();
        boost::asio::async_connect(
            underlyingSocket(),
            endpoints,
            [this, self](boost::system::error_code netEc,
                         const typename NetProtocol::endpoint&)
            {
                if (check(netEc))
                    performTlsHandshake(IsTls{}, socket_);
            });
    }

    template <typename S>
    void performTlsHandshake(FalseType, S& /*socket*/)
    {
        sendRawsocketHandshake();
    }

    template <typename S>
    void performTlsHandshake(TrueType, S& socket)
    {
        auto self = this->shared_from_this();
        socket.async_handshake(
            Socket::client,
            [this, self](boost::system::error_code netEc)
            {
                if (check(netEc))
                    sendRawsocketHandshake();
            });
    }

    void sendRawsocketHandshake()
    {
        handshake_ = RawsockHandshake()
                         .setCodecId(codecId_)
                         .setSizeLimit(settings_->limits().wampReadMsgSize())
                         .toBigEndian();
        auto self = this->shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec))
                    receiveRawsocketHandshake();
            });
    }

    void receiveRawsocketHandshake()
    {
        handshake_ = 0;
        auto self = this->shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec))
                {
                    onRawsocketHandshakeReceived(
                        Handshake::fromBigEndian(handshake_));
                }
            });
    }

    void onRawsocketHandshakeReceived(Handshake hs)
    {
        if (!hs.hasMagicOctet())
        {
            fail(TransportErrc::badHandshake);
            return;
        }

        if (hs.reserved() != 0)
            fail(TransportErrc::badFeature);
        else if (hs.codecId() == codecId_)
            complete(hs);
        else if (hs.hasError())
            fail(hs.errorCode());
        else
            fail(TransportErrc::badHandshake);
    }

    UnderlyingSocket& underlyingSocket()
    {
        return getUnderlyingSocket(IsTls{}, socket_);
    }

    template <typename S>
    static UnderlyingSocket& getUnderlyingSocket(FalseType, S& socket)
    {
        return socket;
    }

    template <typename S>
    static UnderlyingSocket& getUnderlyingSocket(TrueType, S& socket)
    {
        return socket.next_layer();
    }

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
        {
            underlyingSocket().close();
            auto ec = static_cast<std::error_code>(netEc);
            if (netEc == boost::asio::error::operation_aborted)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(makeUnexpected(ec));
        }
        return !netEc;
    }

    void complete(Handshake hs)
    {
        // Clamp send limit to smallest between settings limit and peer limit
        const auto peerLimit = hs.sizeLimit();
        auto txLimit = settings_->limits().wampWriteMsgSize();
        txLimit = txLimit < peerLimit ? txLimit : peerLimit;
        const auto rxLimit = settings_->limits().wampReadMsgSize();
        TransportInfo i{codecId_, txLimit, rxLimit};

        Transporting::Ptr transport =
            std::make_shared<Transport>(std::move(socket_), settings_,
                                        std::move(i), std::move(sslContext_));
        dispatchHandler(std::move(transport));
    }

    void fail(TransportErrc errc)
    {
        underlyingSocket().close();
        dispatchHandler(makeUnexpectedError(errc));
    }

    template <typename TArg>
    void dispatchHandler(TArg&& arg)
    {
        const Handler handler(std::move(handler_));
        handler_ = nullptr;
        handler(std::forward<TArg>(arg));
    }

    Resolver resolver_;
    SslContextType sslContext_;
    Socket socket_;
    Handler handler_;
    std::shared_ptr<Settings> settings_;
    int codecId_ = 0;
    uint32_t handshake_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKCONNECTOR_HPP
