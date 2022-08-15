/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ASIOTRANSPORT_HPP
#define CPPWAMP_INTERNAL_ASIOTRANSPORT_HPP

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
#include "../rawsockoptions.hpp"
#include "rawsockheader.hpp"
#include "transport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Combines a raw socket transport header with an encoded message payload.
//------------------------------------------------------------------------------
class AsioFrame
{
public:
    using Ptr        = std::shared_ptr<AsioFrame>;
    using Header     = uint32_t;
    using GatherBufs = std::array<boost::asio::const_buffer, 2>;

    AsioFrame() = default;

    AsioFrame(RawsockMsgType type, MessageBuffer&& payload)
        : header_(computeHeader(type, payload)),
          payload_(std::move(payload))
    {}

    void clear() {header_ = 0; payload_.clear();}

    void resize(size_t length) {payload_.resize(length);}

    void prepare(RawsockMsgType type, MessageBuffer&& payload)
    {
        header_ = computeHeader(type, payload);
        payload_ = std::move(payload);
    }

    RawsockHeader header() const {return RawsockHeader::fromBigEndian(header_);}

    const MessageBuffer& payload() const & {return payload_;}

    MessageBuffer&& payload() && {return std::move(payload_);}

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
};

//------------------------------------------------------------------------------
template <typename TSocket>
class AsioTransport : public TransportBase
{
public:
    using Socket      = TSocket;
    using SocketPtr   = std::unique_ptr<Socket>;

    static Ptr create(SocketPtr&& s, size_t maxTxLength, size_t maxRxLength)
    {
        return Ptr(new AsioTransport(std::move(s), maxTxLength, maxRxLength));
    }

    bool isOpen() const override {return socket_ && socket_->is_open();}

    bool isStarted() const override {return started_;}

    void start(RxHandler rxHandler, FailHandler failHandler) override
    {
        assert(!started_);
        rxHandler_ = rxHandler;
        failHandler_ = failHandler;
        receive();
        started_ = true;
    }

    void send(MessageBuffer message) override
    {
        assert(started_);
        auto buf = newFrame(RawsockMsgType::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void close() override
    {
        txQueue_.clear();
        rxHandler_ = nullptr;
        if (socket_)
            socket_->close();
    }

    void ping(MessageBuffer message, PingHandler handler) override
    {
        assert(started_);
        pingHandler_ = std::move(handler);
        pingFrame_ = newFrame(RawsockMsgType::ping, std::move(message));
        sendFrame(pingFrame_);
        pingStart_ = std::chrono::high_resolution_clock::now();
    }

protected:
    using TransmitQueue = std::deque<AsioFrame::Ptr>;
    using TimePoint     = std::chrono::high_resolution_clock::time_point;

    AsioTransport(SocketPtr&& socket, size_t maxTxLength, size_t maxRxLength)
        : Base(boost::asio::make_strand(socket->get_executor()),
               maxTxLength, maxRxLength),
          socket_(std::move(socket))
    {}

    AsioFrame::Ptr newFrame(RawsockMsgType type, MessageBuffer&& payload)
    {
        // TODO: Reuse frames somehow
        return std::make_shared<AsioFrame>(type, std::move(payload));
    }

    virtual void sendFrame(AsioFrame::Ptr frame)
    {
        assert(socket_ && "Attempting to send on bad transport");
        assert((frame->payload().size() <= maxSendLength()) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.push_back(std::move(frame));
        transmit();
    }

private:
    using Base = TransportBase;

    void transmit()
    {
        if (isReadyToTransmit())
        {
            txFrame_ = txQueue_.front();
            txQueue_.pop_front();

            auto self = this->shared_from_this();
            boost::asio::async_write(*socket_, txFrame_->gatherBuffers(),
                [this, self](AsioErrorCode ec, size_t size)
                {
                    txFrame_.reset();
                    if (ec)
                    {
                        txQueue_.clear();
                        socket_.reset();
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
                [this, self](AsioErrorCode ec, size_t)
                {
                    if (check(ec))
                        processHeader();
                });
        }
    }

    virtual void processHeader()
    {
        auto hdr = rxFrame_.header();
        auto len  = hdr.length();
        bool ok = check(len <= maxReceiveLength(), TransportErrc::badRxLength)
               && check(hdr.msgTypeIsValid(), RawsockErrc::badMessageType);
        if (ok)
            receivePayload(hdr.msgType(), len);
    }

    void receivePayload(RawsockMsgType msgType, size_t length)
    {
        rxFrame_.resize(length);
        auto self = this->shared_from_this();
        boost::asio::async_read(*socket_, rxFrame_.payloadBuffer(),
            [this, self, msgType](AsioErrorCode ec, size_t)
            {
                if (ec)
                    rxFrame_.clear();
                if (check(ec))
                    switch (msgType)
                    {
                    case RawsockMsgType::wamp:
                        if (rxHandler_)
                        {
                            post(rxHandler_, std::move(rxFrame_).payload());
                        }
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
        auto frame = newFrame(RawsockMsgType::pong,
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

    bool check(AsioErrorCode asioEc)
    {
        if (asioEc)
        {
            if (failHandler_)
            {
                auto ec = make_error_code(
                            static_cast<std::errc>(asioEc.value()));
                post(failHandler_, ec);
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
            if (failHandler_)
                post(failHandler_, make_error_code(errc));
            cleanup();
        }
        return condition;
    }

    void cleanup()
    {
        rxHandler_ = nullptr;
        failHandler_ = nullptr;
        pingHandler_ = nullptr;
        rxFrame_.clear();
        txQueue_.clear();
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        socket_.reset();
    }

    std::unique_ptr<TSocket> socket_;
    bool started_ = false;
    RxHandler rxHandler_;
    FailHandler failHandler_;
    PingHandler pingHandler_;
    AsioFrame rxFrame_;
    TransmitQueue txQueue_;
    AsioFrame::Ptr txFrame_;
    AsioFrame::Ptr pingFrame_;
    TimePoint pingStart_;
    TimePoint pingStop_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ASIOTRANSPORT_HPP
