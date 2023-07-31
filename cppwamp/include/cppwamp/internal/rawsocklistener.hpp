/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_RAWSOCKLISTENER_HPP
#define CPPWAMP_RAWSOCKLISTENER_HPP

#include <memory>
#include <set>
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
struct DefaultRawsockServerOptions
{
    template <typename TSocket, typename TTraits>
    using TransportType = RawsockTransport<TSocket, TTraits>;

    static constexpr bool mockUnresponsiveness() {return false;}

    static uint32_t hostOrderHandshakeBytes(int codecId,
                                            RawsockMaxLength maxRxLength)
    {
        return RawsockHandshake().setCodecId(codecId)
                                 .setMaxLength(maxRxLength)
                                 .toHostOrder();
    }
};

//------------------------------------------------------------------------------
template <typename TAcceptor, typename TConfig = DefaultRawsockServerOptions>
class RawsockListener
    : public std::enable_shared_from_this<RawsockListener<TAcceptor, TConfig>>
{
public:
    using Ptr       = std::shared_ptr<RawsockListener>;
    using Acceptor  = TAcceptor;
    using Settings  = typename Acceptor::Settings;
    using CodecIds  = std::set<int>;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using Socket    = typename Acceptor::Socket;
    using Traits    = typename Acceptor::Traits;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = typename TConfig::template TransportType<Socket, Traits>;

    static Ptr create(IoStrand i, Settings s, CodecIds codecIds)
    {
        return Ptr(new RawsockListener(std::move(i), std::move(s),
                                       std::move(codecIds)));
    }

    void establish(Handler&& handler)
    {
        assert(!handler_ &&
               "RawsockListener establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        acceptor_.establish(
            [this, self](ErrorOr<SocketPtr> socket)
            {
                if (socket)
                {
                    socket_ = std::move(*socket);
                    receiveHandshake();
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
            acceptor_.cancel();
    }

private:
    using Handshake = internal::RawsockHandshake;

    RawsockListener(IoStrand i, Settings s, CodecIds codecIds)
        : codecIds_(std::move(codecIds)),
          acceptor_(std::move(i), std::move(s)),
          maxRxLength_(acceptor_.settings().maxRxLength())
    {}

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
        auto peerCodec = hs.codecId();

        if (!hs.hasMagicOctet())
        {
            fail(TransportErrc::badHandshake);
        }
        else if (hs.reserved() != 0)
        {
            sendHandshake(Handshake::eReservedBitsUsed());
        }
        else if (codecIds_.count(peerCodec) != 0)
        {
            maxTxLength_ = hs.maxLength();
            auto bytes = TConfig::hostOrderHandshakeBytes(peerCodec,
                                                          maxRxLength_);
            if (!TConfig::mockUnresponsiveness())
                sendHandshake(Handshake(bytes));
        }
        else
        {
            sendHandshake(Handshake::eUnsupportedFormat());
        }
    }

    void sendHandshake(Handshake hs)
    {
        handshake_ = hs.toBigEndian();
        auto self = this->shared_from_this();
        boost::asio::async_write(
            *socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self, hs](boost::system::error_code ec, size_t)
            {
                if (check(ec))
                {
                    if (!hs.hasError())
                        complete(hs);
                    else
                        fail(hs.errorCode());
                }
            });
    }

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            socket_.reset();
            auto ec = static_cast<std::error_code>(asioEc);
            if (asioEc == boost::asio::error::operation_aborted)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(UnexpectedError(ec));
        }
        return !asioEc;
    }

    void complete(Handshake hs)
    {
        const TransportInfo i{hs.codecId(),
                              Handshake::byteLengthOf(maxTxLength_),
                              Handshake::byteLengthOf(maxRxLength_)};
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

    CodecIds codecIds_;
    Acceptor acceptor_;
    Handler handler_;
    SocketPtr socket_;
    uint32_t handshake_ = 0;
    RawsockMaxLength maxTxLength_ = {};
    RawsockMaxLength maxRxLength_ = {};
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKLISTENER_HPP
