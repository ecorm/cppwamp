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
template <typename TTraits, typename TResolver, typename TTransport>
struct BasicRawsockConnectorConfig
{
    using Traits = TTraits;
    using Resolver = TResolver;
    using Transport = TTransport;

    // Allows modification of handshake bytes sent by client
    // for testing purposes.
    static uint32_t hostOrderHandshakeBytes(int codecId,
                                            RawsockMaxLength maxRxLength)
    {
        return RawsockHandshake().setCodecId(codecId)
                                 .setMaxLength(maxRxLength)
                                 .toHostOrder();
    }
};

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockConnector
    : public std::enable_shared_from_this<RawsockConnector<TConfig>>
{
public:
    using Ptr      = std::shared_ptr<RawsockConnector>;
    using Settings = typename TConfig::Traits::ClientSettings;
    using Handler  = std::function<void (ErrorOr<Transporting::Ptr>)>;

    static Ptr create(IoStrand i, Settings s, int codecId)
    {
        return Ptr(new RawsockConnector(std::move(i), std::move(s), codecId));
    }

    void establish(Handler handler)
    {
        assert(!handler_ &&
               "RawsockConnector establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        resolver_.resolve(
            settings_,
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
    using Traits         = typename TConfig::Traits;
    using Resolver       = typename TConfig::Resolver;
    using ResolverResult = typename Resolver::Result;
    using NetProtocol    = typename Traits::NetProtocol;
    using Socket         = typename NetProtocol::socket;
    using Transport      = typename TConfig::Transport;
    using Handshake      = internal::RawsockHandshake;

    RawsockConnector(IoStrand i, Settings s, int codecId)
        : settings_(s),
          resolver_(i),
          socket_(std::move(i)),
          codecId_(codecId)
    {}

    void connect(const ResolverResult& endpoints)
    {
        assert(!socket_.is_open());
        settings_.options().applyTo(socket_);

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
        auto bytes = TConfig::hostOrderHandshakeBytes(codecId_, maxRxLength_);
        handshake_ = endian::nativeToBig32(bytes);
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
                              Handshake::byteLengthOf(maxRxLength_),
                              Traits::heartbeatInterval(settings_)};
        Transporting::Ptr transport{Transport::create(std::move(socket_), i)};
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

    Settings settings_;
    Resolver resolver_;
    Socket socket_;
    Handler handler_;
    int codecId_ = 0;
    uint32_t handshake_ = 0;
    RawsockMaxLength maxRxLength_ = {};
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKCONNECTOR_HPP
