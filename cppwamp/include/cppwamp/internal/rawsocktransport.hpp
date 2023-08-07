/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
#define CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <boost/asio/buffer.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
#include "endian.hpp"
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

    RawsockFrame(RawsockMsgType type, MessageBuffer&& payload)
        : payload_(std::move(payload)),
          header_(computeHeader(type, payload_))
    {}

    void clear()
    {
        header_ = 0;
        payload_.clear();
        isPoisoned_ =false;
    }

    void resize(size_t length) {payload_.resize(length);}

    void prepare(RawsockMsgType type, MessageBuffer&& payload)
    {
        header_ = computeHeader(type, payload);
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

    boost::asio::mutable_buffers_1 headerBuffer()
    {
        return boost::asio::buffer(&header_, sizeof(header_));
    }

    boost::asio::mutable_buffers_1 payloadBuffer()
    {
        return boost::asio::buffer(&payload_.front(), payload_.size());
    }

private:
    static Header computeHeader(RawsockMsgType type,
                                const MessageBuffer& payload)
    {
        return RawsockHeader().setMsgType(type)
                              .setLength(payload.size())
                              .toBigEndian();
    }

    MessageBuffer payload_;
    Header header_ = 0;
    bool isPoisoned_ = false;
};

//------------------------------------------------------------------------------
using RawsockPingBytes = std::array<MessageBuffer::value_type,
                                    2*sizeof(uint64_t)>;

//------------------------------------------------------------------------------
class RawsockPingFrame
{
public:
    explicit RawsockPingFrame(uint64_t randomId)
        : baseId_(endian::nativeToBig64(randomId))
    {}

    uint64_t count() const {return sequentialId_;}

    void serialize(RawsockPingBytes& bytes) const
    {
        auto* ptr = bytes.data();
        std::memcpy(ptr, &baseId_, sizeof(baseId_));
        ptr += sizeof(baseId_);
        uint64_t n = endian::nativeToBig64(sequentialId_);
        std::memcpy(ptr, &n, sizeof(sequentialId_));
    }

    void increment() {++sequentialId_;}

private:
    uint64_t baseId_ = 0;
    uint64_t sequentialId_ = 0;
};

//------------------------------------------------------------------------------
class RawsockPinger : public std::enable_shared_from_this<RawsockPinger>
{
public:
    using Handler = std::function<void (ErrorOr<RawsockPingBytes>)>;

    RawsockPinger(IoStrand strand, const TransportInfo& info)
        : timer_(strand),
          frame_(info.randomId),
          interval_(info.heartbeatInterval)
    {}

    void start(Handler handler)
    {
        handler_ = std::move(handler);
        startTimer();
    }

    void stop()
    {
        handler_ = nullptr;
        interval_ = {};
        timer_.cancel();
    }

    void pong(const MessageBuffer& payload)
    {
        if (frame_.count() == 0 || payload.size() != frameBytes_.size())
            return;

        auto cmp = std::memcmp(payload.data(), frameBytes_.data(),
                               frameBytes_.size());
        if (cmp == 0)
            matchingPongReceived_ = true;
    }

private:
    void startTimer()
    {
        std::weak_ptr<RawsockPinger> self = shared_from_this();
        timer_.expires_after(interval_);
        timer_.async_wait(
            [self, this](boost::system::error_code ec)
            {
                static constexpr auto cancelled =
                    boost::asio::error::operation_aborted;
                auto me = self.lock();

                if (!me || ec == cancelled || !handler_)
                    return;

                if (ec)
                {
                    handler_(makeUnexpected(static_cast<std::error_code>(ec)));
                    return;
                }

                if ((frame_.count() > 0) && !matchingPongReceived_)
                {
                    handler_(makeUnexpectedError(
                        TransportErrc::heartbeatTimeout));
                    return;
                }

                matchingPongReceived_ = false;
                frame_.increment();
                frame_.serialize(frameBytes_);
                handler_(frameBytes_);
                startTimer();
            });
    }

    boost::asio::steady_timer timer_;
    Handler handler_;
    RawsockPingFrame frame_;
    RawsockPingBytes frameBytes_ = {};
    Timeout interval_ = {};
    bool matchingPongReceived_ = false;
};

//------------------------------------------------------------------------------
struct DefaultRawsockTransportConfig
{
    // Allows altering transport frame payloads for test purposes.
    static void alter(RawsockMsgType&, MessageBuffer&) {}
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

    TransportInfo info() const override {return info_;}

    bool isRunning() const override {return running_;}

    void start(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        assert(!isRunning());
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;
        std::weak_ptr<Transporting> self = shared_from_this();

        if (pinger_)
        {
            pinger_->start(
                [self, this](ErrorOr<RawsockPingBytes> pingBytes)
                {
                    auto me = self.lock();
                    if (me)
                        onPingFrame(pingBytes);
                });
        }

        receive();
        running_ = true;
    }

    void send(MessageBuffer message) override
    {
        // Due to the handler being posted, the caller may not be aware of a
        // network error having already occurred by time this is called.
        // So do nothing if the transport is already closed.
        if (!isRunning())
            return;

        auto buf = enframe(RawsockMsgType::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void sendNowAndStop(MessageBuffer message) override
    {
        // Due to the handler being posted, the caller may not be aware of a
        // network error having already occurred by time this is called.
        // So do nothing if the transport is already closed.
        if (!isRunning())
            return;

        assert(socket_ && "Attempting to send on bad transport");
        auto frame = enframe(RawsockMsgType::wamp, std::move(message));
        assert((frame->payload().size() <= info_.maxTxLength) &&
               "Outgoing message is longer than allowed by peer");
        frame->poison();
        txQueue_.push_front(std::move(frame));
        transmit();
        running_ = false;
    }

    void stop() override
    {
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        running_ = false;
        if (socket_)
            socket_->close();
        if (pinger_)
            pinger_->stop();
    }

    ConnectionInfo connectionInfo() const override
    {
        if (!socket_)
            return {};
        return TTraits::connectionInfo(socket_->remote_endpoint());
    }

private:
    using Base = Transporting;
    using TransmitQueue = std::deque<RawsockFrame::Ptr>;
    using TimePoint     = std::chrono::high_resolution_clock::time_point;

    RawsockTransport(SocketPtr&& socket, TransportInfo info)
        : strand_(boost::asio::make_strand(socket->get_executor())),
          info_(info),
          socket_(std::move(socket))
    {
        if (timeoutIsDefinite(info_.heartbeatInterval))
            pinger_.reset(new RawsockPinger(strand_, info_));
    }

    void onPingFrame(ErrorOr<RawsockPingBytes> pingBytes)
    {
        if (!isRunning())
            return;

        if (!pingBytes.has_value())
            return fail(pingBytes.error());

        MessageBuffer message{pingBytes->begin(), pingBytes->end()};
        auto buf = enframe(RawsockMsgType::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    RawsockFrame::Ptr enframe(RawsockMsgType type, MessageBuffer&& payload)
    {
        Config::alter(type, payload);
        // TODO: Pool/reuse frames somehow
        return std::make_shared<RawsockFrame>(type, std::move(payload));
    }

    void sendFrame(RawsockFrame::Ptr frame)
    {
        assert(socket_ && "Attempting to send on bad transport");
        assert((frame->payload().size() <= info_.maxTxLength) &&
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
                        stop();
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
            check(len <= info_.maxRxLength, TransportErrc::tooLong) &&
            check(hdr.msgTypeIsValid(), TransportErrc::badCommand);
        if (ok)
            receivePayload(hdr.msgType(), len);
    }

    // NOLINTBEGIN(misc-no-recursion)
    void receivePayload(RawsockMsgType msgType, size_t length)
    {
        rxFrame_.resize(length);
        auto self = this->shared_from_this();
        boost::asio::async_read(*socket_, rxFrame_.payloadBuffer(),
            [this, self, msgType](boost::system::error_code ec, size_t)
            {
                if (ec)
                    rxFrame_.clear();

                if (check(ec) && running_)
                {
                    switch (msgType)
                    {
                    case RawsockMsgType::wamp:
                        if (rxHandler_)
                            post(rxHandler_, std::move(rxFrame_).payload());
                        receive();
                        break;

                    case RawsockMsgType::ping:
                        sendPong();
                        break;

                    case RawsockMsgType::pong:
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
        auto frame = enframe(RawsockMsgType::pong,
                             std::move(rxFrame_).payload());
        sendFrame(std::move(frame));
        receive();
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    void receivePong()
    {
        // Unsolicited pongs may serve as unidirectional heartbeats.
        // https://github.com/wamp-proto/wamp-proto/issues/274#issuecomment-288626150
        if (pinger_ != nullptr)
            pinger_->pong(rxFrame_.payload());

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
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        rxFrame_.clear();
        txQueue_.clear();
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        socket_.reset();
        pinger_.reset();
        running_ = false;
    }

    IoStrand strand_;
    TransportInfo info_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    RawsockFrame rxFrame_;
    TransmitQueue txQueue_;
    MessageBuffer pingBuffer_;
    RawsockFrame::Ptr txFrame_;
    RawsockFrame::Ptr pingFrame_;
    std::unique_ptr<TSocket> socket_;
    std::unique_ptr<RawsockPinger> pinger_;
    uint64_t pingId_ = 0;
    bool running_ = false;
    bool matchingPongReceived_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
