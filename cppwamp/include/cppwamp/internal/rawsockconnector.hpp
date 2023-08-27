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
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
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
    template <typename TSocket, typename TTraits>
    using TransportType = RawsockTransport<TSocket, TTraits>;

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
    using Traits    = typename Opener::Traits;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = typename TConfig::template TransportType<Socket, Traits>;

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
        : opener_(std::move(i), std::move(s)),
          codecId_(codecId),
          heartbeatInterval_(Traits::heartbeatInterval(opener_.settings())),
          maxRxLength_(opener_.settings().maxRxLength())
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
            socket_.reset();
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
                              heartbeatInterval_};
        Transporting::Ptr transport{Transport::create(std::move(socket_), i)};
        socket_.reset();
        dispatchHandler(std::move(transport));
    }

    void fail(TransportErrc errc)
    {
        socket_.reset();
        dispatchHandler(makeUnexpectedError(errc));
    }

    template <typename TArg>
    void dispatchHandler(TArg&& arg)
    {
        const Handler handler(std::move(handler_));
        handler_ = nullptr;
        handler(std::forward<TArg>(arg));
    }

    Opener opener_;
    SocketPtr socket_;
    Handler handler_;
    int codecId_ = 0;
    Timeout heartbeatInterval_ = unspecifiedTimeout;
    uint32_t handshake_ = 0;
    RawsockMaxLength maxRxLength_ = {};
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKCONNECTOR_HPP
