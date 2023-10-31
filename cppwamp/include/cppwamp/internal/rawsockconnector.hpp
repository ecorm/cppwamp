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
    using Resolver  = TResolver;
    using Ptr       = std::shared_ptr<RawsockConnector>;
    using Settings  = typename Resolver::Settings;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;

    RawsockConnector(IoStrand i, Settings s, int codecId)
        : resolver_(i),
          socket_(std::move(i)),
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
        socket_.close();
    }

private:
    using Traits         = typename Resolver::Traits;
    using Transport      = typename Resolver::Transport;
    using ResolverResult = typename Resolver::Result;
    using NetProtocol    = typename Traits::NetProtocol;
    using Socket         = typename NetProtocol::socket;
    using Handshake      = internal::RawsockHandshake;

    void connect(const ResolverResult& endpoints)
    {
        assert(!socket_.is_open());
        settings_->socketOptions().applyTo(socket_);

        auto self = this->shared_from_this();
        boost::asio::async_connect(
            socket_,
            endpoints,
            [this, self](boost::system::error_code netEc,
                         const typename NetProtocol::endpoint&)
            {
                if (check(netEc))
                    sendHandshake();
            });
    }

    void sendHandshake()
    {
        handshake_ = RawsockHandshake()
                         .setCodecId(codecId_)
                         .setMaxLength(settings_->limits().bodySize())
                         .toBigEndian();
        auto self = this->shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec))
                    receiveHandshake();
            });
    }

    void receiveHandshake()
    {
        handshake_ = 0;
        auto self = this->shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec))
                    onHandshakeReceived(Handshake::fromBigEndian(handshake_));
            });
    }

    void onHandshakeReceived(Handshake hs)
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

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
        {
            socket_.close();
            auto ec = static_cast<std::error_code>(netEc);
            if (netEc == boost::asio::error::operation_aborted)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(makeUnexpected(ec));
        }
        return !netEc;
    }

    void complete(Handshake hs)
    {
        const TransportInfo i{codecId_,
                              hs.maxLengthInBytes(),
                              settings_->limits().bodySize(),
                              Traits::heartbeatInterval(*settings_)};
        Transporting::Ptr transport =
            std::make_shared<Transport>(std::move(socket_), settings_, i);
        dispatchHandler(std::move(transport));
    }

    void fail(TransportErrc errc)
    {
        socket_.close();
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
    Socket socket_;
    Handler handler_;
    std::shared_ptr<Settings> settings_;
    int codecId_ = 0;
    uint32_t handshake_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKCONNECTOR_HPP
