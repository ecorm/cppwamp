/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_RAWSOCKCONNECTOR_HPP
#define CPPWAMP_RAWSOCKCONNECTOR_HPP

#include <cassert>
#include <memory>
#include <utility>
#include "../asiodefs.hpp"
#include "../erroror.hpp"
#include "rawsockhandshake.hpp"
#include "rawsocktransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct DefaultRawsockClientConfig
{
    template <typename TSocket>
    using TransportType = RawsockTransport<TSocket>;

    static uint32_t hostOrderHandshakeBytes(int codecId,
                                            RawsockMaxLength maxRxLength)
    {
        return RawsockHandshake().setCodecId(codecId)
                                 .setMaxLength(maxRxLength)
                                 .toHostOrder();
    }
};

//------------------------------------------------------------------------------
template <typename TOpener, typename TConfig = DefaultRawsockClientConfig>
class RawsockConnector
    : public std::enable_shared_from_this<RawsockConnector<TOpener, TConfig>>
{
public:
    using Ptr       = std::shared_ptr<RawsockConnector>;
    using Opener    = TOpener;
    using Settings  = typename Opener::Settings;
    using Socket    = typename Opener::Socket;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = typename TConfig::template TransportType<Socket>;

    static Ptr create(IoStrand i, Settings s, int codecId)
    {
        return Ptr(new RawsockConnector(std::move(i), std::move(s), codecId));
    }

    void establish(Handler&& handler)
    {
        assert(!handler_ &&
               "RawsockConnector establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        opener_.establish(
            [this, self](ErrorOr<SocketPtr> socket)
            {
                if (socket)
                {
                    socket_ = std::move(*socket);
                    sendHandshake();
                }
                else
                {
                    auto ec = socket.error();
                    if (ec == std::errc::operation_canceled)
                        ec = make_error_code(TransportErrc::aborted);
                    dispatchHandler(UnexpectedError(ec));
                }
            }
        );
    }

    void cancel()
    {
        if (socket_)
            socket_->close();
        else
            opener_.cancel();
    }

private:
    using Handshake = internal::RawsockHandshake;

    RawsockConnector(IoStrand i, Settings s, int codecId)
        : codecId_(codecId),
          maxRxLength_(s.maxRxLength()),
          opener_(std::move(i), std::move(s))
    {}

    void sendHandshake()
    {
        auto bytes = TConfig::hostOrderHandshakeBytes(codecId_, maxRxLength_);
        handshake_ = endian::nativeToBig32(bytes);
        auto self = this->shared_from_this();
        boost::asio::async_write(
            *socket_,
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
            *socket_,
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
            fail(RawsockErrc::badHandshake);
        else if (hs.reserved() != 0)
            fail(RawsockErrc::reservedBitsUsed);
        else if (hs.codecId() == codecId_)
            complete(hs);
        else if (hs.hasError())
            fail(hs.errorCode());
        else
            fail(RawsockErrc::badHandshake);
    }

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            socket_.reset();
            auto ec = make_error_code(static_cast<std::errc>(asioEc.value()));
            if (ec == std::errc::operation_canceled)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(makeUnexpected(ec));
        }
        return !asioEc;
    }

    void complete(Handshake hs)
    {
        TransportInfo i{codecId_,
                        hs.maxLengthInBytes(),
                        Handshake::byteLengthOf(maxRxLength_)};
        Transporting::Ptr transport{Transport::create(std::move(socket_), i)};
        socket_.reset();
        dispatchHandler(std::move(transport));
    }

    void fail(RawsockErrc errc)
    {
        socket_.reset();
        dispatchHandler(makeUnexpectedError(errc));
    }

    template <typename TArg>
    void dispatchHandler(TArg&& arg)
    {
        Handler handler(std::move(handler_));
        handler_ = nullptr;
        handler(std::forward<TArg>(arg));
    }

    SocketPtr socket_;
    Handler handler_;
    int codecId_;
    RawsockMaxLength maxRxLength_;
    uint32_t handshake_;
    Opener opener_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKCONNECTOR_HPP
