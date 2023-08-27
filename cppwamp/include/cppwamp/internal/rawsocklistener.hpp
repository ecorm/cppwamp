/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKLISTENER_HPP
#define CPPWAMP_INTERNAL_RAWSOCKLISTENER_HPP

#include <memory>
#include <set>
#include <utility>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../listener.hpp"
#include "rawsockhandshake.hpp"
#include "rawsocktransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct DefaultRawsockServerConfig
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
template <typename TAcceptor, typename TConfig = DefaultRawsockServerConfig>
class RawsockListener
    : public std::enable_shared_from_this<RawsockListener<TAcceptor, TConfig>>
{
public:
    using Ptr       = std::shared_ptr<RawsockListener>;
    using Acceptor  = TAcceptor;
    using Settings  = typename Acceptor::Settings;
    using CodecIds  = std::set<int>;
    using Traits    = typename Acceptor::Traits;
    using Handler   = Listening::Handler;

    static Ptr create(AnyIoExecutor e, IoStrand i, Settings s,
                      CodecIds codecIds)
    {
        return Ptr(new RawsockListener(std::move(e), std::move(i), std::move(s),
                                       std::move(codecIds)));
    }

    void observe(Handler handler) {handler_ = handler;}

    void establish()
    {
        assert(!establishing_ && "RawsockListener already establishing");
        establishing_ = true;
        auto self = this->shared_from_this();
        acceptor_.establish(
            [this, self](AcceptorResult result)
            {
                if (checkAcceptorError(result))
                {
                    socket_ = std::move(result.socket);
                    receiveHandshake();
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
    using Socket         = typename Acceptor::Socket;
    using SocketPtr      = std::unique_ptr<Socket>;
    using Handshake      = internal::RawsockHandshake;
    using ErrorCat       = ListeningErrorCategory;
    using AcceptorResult = typename Acceptor::Result;
    using Transport = typename TConfig::template TransportType<Socket, Traits>;

    RawsockListener(AnyIoExecutor e, IoStrand i, Settings s, CodecIds codecIds)
        : codecIds_(std::move(codecIds)),
          acceptor_(std::move(e), std::move(i), std::move(s)),
          maxRxLength_(acceptor_.settings().maxRxLength())
    {}

    void receiveHandshake()
    {
        // TODO: Timeout waiting for handshake
        handshake_ = 0;
        auto self = this->shared_from_this();
        boost::asio::async_read(
            *socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (checkReadError(ec))
                    onHandshakeReceived(Handshake::fromBigEndian(handshake_));
            });
    }

    void onHandshakeReceived(Handshake hs)
    {
        auto peerCodec = hs.codecId();

        if (!hs.hasMagicOctet())
        {
            fail(TransportErrc::badHandshake, ErrorCat::transient, "handshake");
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
                onHandshakeSent(hs, ec);
            });
    }

    void onHandshakeSent(Handshake hs, boost::system::error_code ec)
    {
        if (!checkWriteError(ec))
            return;
        if (!hs.hasError())
            complete(hs);
        else
            fail(hs.errorCode(), ErrorCat::transient, "rawsock handshake");
    }

    bool checkAcceptorError(const AcceptorResult& result)
    {
        if (result.socket != nullptr)
            return true;

        auto ec = result.error;
        if (ec == std::errc::operation_canceled)
            ec = make_error_code(TransportErrc::aborted);
        fail(ec, result.category, result.operation);
        return false;
    }

    bool checkReadError(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        auto ec = static_cast<std::error_code>(netEc);
        auto cat = SocketErrorHelper::isReceiveFatalError(netEc)
                       ? ListeningErrorCategory::transient
                       : ListeningErrorCategory::fatal;
        if (netEc == std::errc::operation_canceled)
            ec = make_error_code(TransportErrc::aborted);
        fail(ec, cat, "socket recv");
        return false;
    }

    bool checkWriteError(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        auto ec = static_cast<std::error_code>(netEc);
        auto cat = SocketErrorHelper::isSendFatalError(netEc)
                       ? ListeningErrorCategory::transient
                       : ListeningErrorCategory::fatal;
        if (netEc == std::errc::operation_canceled)
            ec = make_error_code(TransportErrc::aborted);
        fail(ec, cat, "socket send");
        return false;
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

    void fail(std::error_code ec, ListeningErrorCategory cat, const char* op)
    {
        socket_.reset();
        dispatchHandler({ec, cat, op});
    }

    void fail(TransportErrc errc, ListeningErrorCategory cat, const char* op)
    {
        fail(make_error_code(errc), cat, op);
    }

    void dispatchHandler(ListenResult result)
    {
        if (handler_)
            handler_(std::move(result));
        establishing_ = false;
    }

    CodecIds codecIds_;
    Acceptor acceptor_;
    Handler handler_;
    SocketPtr socket_;
    uint32_t handshake_ = 0;
    RawsockMaxLength maxTxLength_ = {};
    RawsockMaxLength maxRxLength_ = {};
    bool establishing_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKLISTENER_HPP
