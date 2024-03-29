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
#include "../erroror.hpp"
#include "rawsockhandshake.hpp"
#include "rawsocktransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct DefaultRawsockServerConfig
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
template <typename TAcceptor, typename TConfig = DefaultRawsockServerConfig>
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
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = typename TConfig::template TransportType<Socket>;

    static Ptr create(IoStrand i, Settings s, CodecIds codecIds)
    {
        using std::move;
        return Ptr(new RawsockListener(move(i), move(s), move(codecIds)));
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
          maxRxLength_(s.maxRxLength()),
          acceptor_(std::move(i), std::move(s))
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
            fail(RawsockErrc::badHandshake);
        else if (hs.reserved() != 0)
            sendHandshake(Handshake::eReservedBitsUsed());
        else if (codecIds_.count(peerCodec))
        {
            maxTxLength_ = hs.maxLength();
            auto bytes = TConfig::hostOrderHandshakeBytes(peerCodec,
                                                          maxRxLength_);
            sendHandshake(Handshake(bytes));
        }
        else
            sendHandshake(Handshake::eUnsupportedFormat());
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
            auto ec = make_error_code(static_cast<std::errc>(asioEc.value()));
            if (ec == std::errc::operation_canceled)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(UnexpectedError(ec));
        }
        return !asioEc;
    }

    void complete(Handshake hs)
    {
        TransportInfo i{hs.codecId(),
                        Handshake::byteLengthOf(maxTxLength_),
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
    CodecIds codecIds_;
    RawsockMaxLength maxTxLength_;
    RawsockMaxLength maxRxLength_;
    uint32_t handshake_;
    Acceptor acceptor_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKLISTENER_HPP
