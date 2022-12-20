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
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <boost/asio/buffer.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include "../asiodefs.hpp"
#include "../error.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
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
        : header_(computeHeader(type, payload)),
          payload_(std::move(payload))
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

    Header header_;
    MessageBuffer payload_;
    bool isPoisoned_ = false;
};

//------------------------------------------------------------------------------
struct DefaultRawsockTransportConfig
{
    static RawsockFrame::Ptr enframe(RawsockMsgType type,
                                     MessageBuffer&& payload)
    {
        // TODO: Reuse frames somehow
        return std::make_shared<RawsockFrame>(type, std::move(payload));
    }
};

//------------------------------------------------------------------------------
template <typename TSocket, typename TConfig = DefaultRawsockTransportConfig>
class RawsockTransport : public Transporting
{
public:
    using Ptr            = std::shared_ptr<RawsockTransport>;
    using Socket         = TSocket;
    using Config         = TConfig;
    using SocketPtr      = std::unique_ptr<Socket>;
    using RxHandler      = typename Transporting::RxHandler;
    using TxErrorHandler = typename Transporting::TxErrorHandler;
    using PingHandler    = typename Transporting::PingHandler;

    static Ptr create(SocketPtr&& s, TransportInfo info)
    {
        return Ptr(new RawsockTransport(std::move(s), info));
    }

    TransportInfo info() const override {return info_;}

    bool isStarted() const override {return running_;}

    void start(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        assert(!isStarted());
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;
        receive();
        running_ = true;
    }

    void send(MessageBuffer message) override
    {
        assert(isStarted());
        auto buf = enframe(RawsockMsgType::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void sendNowAndClose(MessageBuffer message) override
    {
        assert(isStarted());
        assert(socket_ && "Attempting to send on bad transport");
        auto frame = enframe(RawsockMsgType::wamp, std::move(message));
        assert((frame->payload().size() <= info_.maxTxLength) &&
               "Outgoing message is longer than allowed by peer");
        frame->poison();
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void close() override
    {
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        running_ = false;
        if (socket_)
            socket_->close();
    }

    void ping(MessageBuffer message, PingHandler handler) override
    {
        assert(isStarted());
        pingHandler_ = std::move(handler);
        pingFrame_ = enframe(RawsockMsgType::ping, std::move(message));
        sendFrame(pingFrame_);
        pingStart_ = std::chrono::high_resolution_clock::now();
    }

private:
    using Base = Transporting;
    using TransmitQueue = std::deque<RawsockFrame::Ptr>;
    using TimePoint     = std::chrono::high_resolution_clock::time_point;

    RawsockTransport(SocketPtr&& socket, TransportInfo info)
        : strand_(boost::asio::make_strand(socket->get_executor())),
        socket_(std::move(socket)),
        info_(info)
    {}

    RawsockFrame::Ptr enframe(RawsockMsgType type, MessageBuffer&& payload)
    {
        return Config::enframe(type, std::move(payload));
    }

    void sendFrame(RawsockFrame::Ptr frame)
    {
        assert(socket_ && "Attempting to send on bad transport");
        assert((frame->payload().size() <= info_.maxTxLength) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.push_back(std::move(frame));
        transmit();
    }

    void transmit()
    {
        if (isReadyToTransmit())
        {
            txFrame_ = txQueue_.front();
            txQueue_.pop_front();

            auto self = this->shared_from_this();
            boost::asio::async_write(*socket_, txFrame_->gatherBuffers(),
                [this, self](boost::system::error_code asioEc, size_t size)
                {
                    bool frameWasPoisoned = txFrame_ && txFrame_->isPoisoned();
                    txFrame_.reset();
                    if (asioEc)
                    {
                        if (txErrorHandler_)
                        {
                            auto ec = make_error_code(
                                static_cast<std::errc>(asioEc.value()));
                            txErrorHandler_(ec);
                        }
                        cleanup();
                    }
                    else if (frameWasPoisoned)
                    {
                        close();
                    }
                    else
                    {
                        transmit();
                    }
                });
        }
    }

    bool isReadyToTransmit() const
    {
        return socket_ &&          // Socket is still open
               !txFrame_ &&        // No async_write is in progress
               !txQueue_.empty();  // One or more messages are enqueued
    }

    void receive()
    {
        if (socket_)
        {
            rxFrame_.clear();
            auto self = this->shared_from_this();
            boost::asio::async_read(*socket_, rxFrame_.headerBuffer(),
                [this, self](boost::system::error_code ec, size_t)
                {
                    if (check(ec))
                        processHeader();
                });
        }
    }

    void processHeader()
    {
        auto hdr = rxFrame_.header();
        auto len  = hdr.length();
        bool ok = check(len <= info_.maxRxLength, TransportErrc::badRxLength)
               && check(hdr.msgTypeIsValid(), RawsockErrc::badMessageType);
        if (ok)
            receivePayload(hdr.msgType(), len);
    }

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
                    }
            });
    }

    void sendPong()
    {
        auto frame = enframe(RawsockMsgType::pong,
                              std::move(rxFrame_).payload());
        sendFrame(std::move(frame));
        receive();
    }

    void receivePong()
    {
        if (canProcessPong())
        {
            namespace chrn = std::chrono;
            pingStop_ = chrn::high_resolution_clock::now();
            using Fms = chrn::duration<float, chrn::milliseconds::period>;
            float elapsed = Fms(pingStop_ - pingStart_).count();
            post(pingHandler_, elapsed);
            pingHandler_ = nullptr;
        }
        pingFrame_.reset();
        receive();
    }

    bool canProcessPong() const
    {
        return pingHandler_ && pingFrame_ &&
               (rxFrame_.payload() == pingFrame_->payload());
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        boost::asio::post(strand_, std::bind(std::forward<F>(handler),
                                             std::forward<Ts>(args)...));
    }

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            if (rxHandler_)
            {
                auto ec = make_error_code(
                            static_cast<std::errc>(asioEc.value()));
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

    void cleanup()
    {
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        pingHandler_ = nullptr;
        rxFrame_.clear();
        txQueue_.clear();
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        socket_.reset();
    }

    IoStrand strand_;
    std::unique_ptr<TSocket> socket_;
    TransportInfo info_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    PingHandler pingHandler_;
    RawsockFrame rxFrame_;
    TransmitQueue txQueue_;
    RawsockFrame::Ptr txFrame_;
    RawsockFrame::Ptr pingFrame_;
    TimePoint pingStart_;
    TimePoint pingStop_;
    bool running_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
