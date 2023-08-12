/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
#define CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../transport.hpp"
#include "pinger.hpp"
#include "rawsockheader.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Combines a raw socket transport header with an encoded message payload.
//------------------------------------------------------------------------------
class RawsockFrame
{
public:
    using Ptr        = std::shared_ptr<RawsockFrame>;
    using Header     = uint32_t;
    using GatherBufs = std::array<boost::asio::const_buffer, 2>;

    RawsockFrame() = default;

    RawsockFrame(RawsockMsgKind kind, MessageBuffer&& payload)
        : payload_(std::move(payload)),
          header_(computeHeader(kind, payload_))
    {}

    void clear()
    {
        header_ = 0;
        payload_.clear();
        isPoisoned_ =false;
    }

    void resize(size_t length) {payload_.resize(length);}

    void prepare(RawsockMsgKind kind, MessageBuffer&& payload)
    {
        header_ = computeHeader(kind, payload);
        payload_ = std::move(payload);
    }

    RawsockHeader header() const {return RawsockHeader::fromBigEndian(header_);}

    const MessageBuffer& payload() const & {return payload_;}

    MessageBuffer&& payload() && {return std::move(payload_);}

    void poison(bool poisoned = true) {isPoisoned_ = poisoned;}

    bool isPoisoned() const {return isPoisoned_;}

    GatherBufs gatherBuffers()
    {
        return GatherBufs{{ {&header_, sizeof(header_)},
                            {payload_.data(), payload_.size()} }};
    }

    boost::asio::mutable_buffer headerBuffer()
    {
        return boost::asio::buffer(&header_, sizeof(header_));
    }

    boost::asio::mutable_buffer payloadBuffer()
    {
        return boost::asio::buffer(&payload_.front(), payload_.size());
    }

private:
    static Header computeHeader(RawsockMsgKind kind,
                                const MessageBuffer& payload)
    {
        return RawsockHeader().setMsgKind(kind)
                              .setLength(payload.size())
                              .toBigEndian();
    }

    MessageBuffer payload_;
    Header header_ = 0;
    bool isPoisoned_ = false;
};

//------------------------------------------------------------------------------
struct DefaultRawsockTransportConfig
{
    // Allows pre-processing transport frame payloads for test purposes.
    static void preProcess(RawsockMsgKind&, MessageBuffer&) {}
};

//------------------------------------------------------------------------------
template <typename TSocket, typename TTraits,
          typename TConfig = DefaultRawsockTransportConfig>
class RawsockTransport : public Transporting
{
public:
    using Ptr            = std::shared_ptr<RawsockTransport>;
    using Socket         = TSocket;
    using Config         = TConfig;
    using SocketPtr      = std::unique_ptr<Socket>;
    using RxHandler      = typename Transporting::RxHandler;
    using TxErrorHandler = typename Transporting::TxErrorHandler;

    static Ptr create(SocketPtr&& s, TransportInfo info)
    {
        return Ptr(new RawsockTransport(std::move(s), info));
    }

private:
    using Base = Transporting;
    using TransmitQueue = std::deque<RawsockFrame::Ptr>;

    RawsockTransport(SocketPtr&& socket, TransportInfo info)
        : Base(info, TTraits::connectionInfo(socket->remote_endpoint())),
          strand_(boost::asio::make_strand(socket->get_executor())),
          socket_(std::move(socket))
    {
        if (timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<Pinger>(strand_, Base::info());
    }

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;

        if (pinger_)
        {
            std::weak_ptr<Transporting> self = shared_from_this();
            pinger_->start(
                [self, this](ErrorOr<PingBytes> pingBytes)
                {
                    auto me = self.lock();
                    if (me)
                        onPingFrame(pingBytes);
                });
        }

        receive();
    }

    void onSend(MessageBuffer message) override
    {
        if (!socket_)
            return;
        auto buf = enframe(RawsockMsgKind::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void onSendNowAndStop(MessageBuffer message) override
    {
        if (!socket_)
            return;
        auto frame = enframe(RawsockMsgKind::wamp, std::move(message));
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        frame->poison();
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void onStop() override
    {
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        if (socket_)
            socket_->close();
        if (pinger_)
            pinger_->stop();
    }

    void onPingFrame(ErrorOr<PingBytes> pingBytes)
    {
        if (state() != Transporting::State::running)
            return;

        if (!pingBytes.has_value())
        {
            return fail(pingBytes.error());
        }

        MessageBuffer message{pingBytes->begin(), pingBytes->end()};
        auto buf = enframe(RawsockMsgKind::ping, std::move(message));
        sendFrame(std::move(buf));
    }

    RawsockFrame::Ptr enframe(RawsockMsgKind kind, MessageBuffer&& payload)
    {
        Config::preProcess(kind, payload);
        // TODO: Pool/reuse frames somehow
        return std::make_shared<RawsockFrame>(kind, std::move(payload));
    }

    void sendFrame(RawsockFrame::Ptr frame)
    {
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.push_back(std::move(frame));
        transmit();
    }

    // NOLINTBEGIN(misc-no-recursion)
    void transmit()
    {
        if (isReadyToTransmit())
        {
            txFrame_ = txQueue_.front();
            txQueue_.pop_front();

            auto self = this->shared_from_this();
            boost::asio::async_write(*socket_, txFrame_->gatherBuffers(),
                [this, self](boost::system::error_code asioEc, size_t)
                {
                    const bool frameWasPoisoned = txFrame_ &&
                                                  txFrame_->isPoisoned();
                    txFrame_.reset();
                    if (asioEc)
                    {
                        if (txErrorHandler_)
                        {
                            auto ec = static_cast<std::error_code>(asioEc);
                            txErrorHandler_(ec);
                        }
                        cleanup();
                    }
                    else if (frameWasPoisoned)
                    {
                        onStop();
                    }
                    else
                    {
                        transmit();
                    }
                });
        }
    }
    // NOLINTEND(misc-no-recursion)

    bool isReadyToTransmit() const
    {
        return socket_ &&          // Socket is still open
               !txFrame_ &&        // No async_write is in progress
               !txQueue_.empty();  // One or more messages are enqueued
    }

    // NOLINTBEGIN(misc-no-recursion)
    void receive()
    {
        if (socket_)
        {
            rxFrame_.clear();
            auto self = this->shared_from_this();
            boost::asio::async_read(*socket_, rxFrame_.headerBuffer(),
                [this, self](boost::system::error_code ec, size_t)
                {
                    if (ec == boost::asio::error::connection_reset ||
                        ec == boost::asio::error::eof)
                    {
                        onRemoteDisconnect();
                    }
                    else if (check(ec))
                    {
                        processHeader();
                    }
                });
        }
    }
    // NOLINTEND(misc-no-recursion)

    void onRemoteDisconnect()
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpectedError(TransportErrc::disconnected));
        cleanup();
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    void processHeader()
    {
        const auto hdr = rxFrame_.header();
        const auto len  = hdr.length();
        const bool ok =
            check(len <= info().maxRxLength(), TransportErrc::inboundTooLong) &&
            check(hdr.msgTypeIsValid(), TransportErrc::badCommand);
        if (ok)
            receivePayload(hdr.msgKind(), len);
    }

    // NOLINTBEGIN(misc-no-recursion)
    void receivePayload(RawsockMsgKind kind, size_t length)
    {
        rxFrame_.resize(length);
        auto self = this->shared_from_this();
        boost::asio::async_read(*socket_, rxFrame_.payloadBuffer(),
            [this, self, kind](boost::system::error_code ec, size_t)
            {
                if (ec)
                    rxFrame_.clear();

                if (check(ec) && state() == Transporting::State::running)
                {
                    switch (kind)
                    {
                    case RawsockMsgKind::wamp:
                        if (rxHandler_)
                            post(rxHandler_, std::move(rxFrame_).payload());
                        receive();
                        break;

                    case RawsockMsgKind::ping:
                        sendPong();
                        break;

                    case RawsockMsgKind::pong:
                        receivePong();
                        break;

                    default:
                        assert(false);
                        break;
                    }
                }
            });
    }
    // NOLINTEND(misc-no-recursion)

    // NOLINTNEXTLINE(misc-no-recursion)
    void sendPong()
    {
        auto f = enframe(RawsockMsgKind::pong, std::move(rxFrame_).payload());
        sendFrame(std::move(f));
        receive();
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    void receivePong()
    {
        // Unsolicited pongs may serve as unidirectional heartbeats.
        // https://github.com/wamp-proto/wamp-proto/issues/274#issuecomment-288626150
        if (pinger_ != nullptr)
            pinger_->pong(rxFrame_.payload().data(), rxFrame_.payload().size());

        receive();
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        // NOLINTNEXTLINE(modernize-avoid-bind)
        boost::asio::post(strand_, std::bind(std::forward<F>(handler),
                                             std::forward<Ts>(args)...));
    }

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            if (rxHandler_)
            {
                auto ec = static_cast<std::error_code>(asioEc);
                post(rxHandler_, UnexpectedError(ec));
            }
            cleanup();
        }
        return !asioEc;
    }

    template <typename TErrc>
    bool check(bool condition, TErrc errc)
    {
        if (!condition)
        {
            if (rxHandler_)
                post(rxHandler_, makeUnexpectedError(errc));
            cleanup();
        }
        return condition;
    }

    void fail(std::error_code ec)
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpected(ec));
        cleanup();
    }

    void cleanup()
    {
        Base::shutdown();
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        rxFrame_.clear();
        txQueue_.clear();
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        socket_.reset();
        pinger_.reset();
    }

    IoStrand strand_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    RawsockFrame rxFrame_;
    TransmitQueue txQueue_;
    MessageBuffer pingBuffer_;
    RawsockFrame::Ptr txFrame_;
    RawsockFrame::Ptr pingFrame_;
    std::unique_ptr<TSocket> socket_;
    std::shared_ptr<Pinger> pinger_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
