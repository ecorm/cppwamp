/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASIOTRANSPORT_HPP
#define CPPWAMP_ASIOTRANSPORT_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include "../asiodefs.hpp"
#include "../error.hpp"
#include "../rawsockoptions.hpp"
#include "rawsockheader.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class AsioBuffer
{
public:
    void write(const char* data, size_t length) {payload_.append(data, length);}

    const char* data() const {return payload_.data();}

    size_t length() const {return payload_.length();}

private:
    using Header = uint32_t;
    using GatherBufs = std::array<boost::asio::const_buffer, 2>;

    void clear() {header_ = 0; payload_.clear();}

    void resize(size_t length) {payload_.resize(length);}

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
        return boost::asio::buffer(&payload_.front(), payload_.length());
    }

    RawsockHeader header() const {return RawsockHeader::fromBigEndian(header_);}

    void prepare(RawsockMsgType type)
    {
        header_ = RawsockHeader().setMsgType(type)
                                 .setLength(payload_.length())
                                 .toBigEndian();
    }

    Header header_;
    std::string payload_;

    template <typename> friend class AsioTransport;
};

//------------------------------------------------------------------------------
template <typename TSocket>
class AsioTransport :
        public std::enable_shared_from_this<AsioTransport<TSocket>>
{
public:
    using Ptr         = std::shared_ptr<AsioTransport>;
    using Buffer      = std::shared_ptr<AsioBuffer>;
    using RxHandler   = std::function<void (Buffer)>;
    using FailHandler = std::function<void (std::error_code ec)>;
    using PingHandler = std::function<void (float)>;

    using Socket      = TSocket;
    using SocketPtr   = std::unique_ptr<Socket>;

    static Ptr create(SocketPtr&& socket, size_t maxTxLength,
                      size_t maxRxLength)
    {
        return Ptr(new AsioTransport(std::move(socket), maxTxLength,
                                     maxRxLength));
    }

    // Noncopyable
    AsioTransport(const AsioTransport&) = delete;
    AsioTransport& operator=(const AsioTransport&) = delete;

    virtual ~AsioTransport() {}

    size_t maxSendLength() const {return maxTxLength_;}

    size_t maxReceiveLength() const {return maxRxLength_;}

    bool isOpen() const {return socket_ && socket_->is_open();}

    bool isStarted() const {return started_;}

    void start(RxHandler rxHandler, FailHandler failHandler)
    {
        assert(!started_);
        rxHandler_ = rxHandler;
        failHandler_ = failHandler;
        receive();
        started_ = true;
    }

    // TODO: Re-use buffers somehow
    Buffer getBuffer() {return std::make_shared<AsioBuffer>();}

    void send(Buffer message)
    {
        assert(started_);
        sendMessage(RawsockMsgType::wamp, std::move(message));
    }

    void close()
    {
        txQueue_ = TransmitQueue();
        rxHandler_ = nullptr;
        if (socket_)
            socket_->close();
    }

    AnyExecutor executor() const {return executor_;}

    void ping(Buffer message, PingHandler handler)
    {
        assert(started_);
        pingHandler_ = std::move(handler);
        pingBuffer_ = std::move(message);
        sendMessage(RawsockMsgType::ping, pingBuffer_);
        pingStart_ = std::chrono::high_resolution_clock::now();
    }

protected:
    using TransmitQueue = std::queue<Buffer>;
    using TimePoint     = std::chrono::high_resolution_clock::time_point;

    AsioTransport(SocketPtr&& socket, size_t maxTxLength, size_t maxRxLength)
        : socket_(std::move(socket)),
          executor_(socket_->get_executor()),
          maxTxLength_(maxTxLength),
          maxRxLength_(maxRxLength)
    {}

    virtual void sendMessage(RawsockMsgType type, Buffer message)
    {
        assert(socket_ && "Attempting to send on bad transport");
        assert((message->length() <= maxTxLength_) &&
               "Outgoing message is longer than allowed by peer");

        message->prepare(type);
        txQueue_.push(std::move(message));
        transmit();
    }

private:
    using Executor = typename TSocket::executor_type;

    template <typename TFunctor>
    void post(TFunctor&& fn)
    {
        boost::asio::post(executor_, std::forward<TFunctor>(fn));
    }

    void transmit()
    {
        if (isReadyToTransmit())
        {
            txBuffer_ = txQueue_.front();
            txQueue_.pop();

            auto self = this->shared_from_this();
            boost::asio::async_write(*socket_, txBuffer_->gatherBuffers(),
                [this, self](AsioErrorCode ec, size_t size)
                {
                    txBuffer_.reset();
                    if (ec)
                    {
                        txQueue_ = TransmitQueue();
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
        return socket_ &&           // Socket is still open
               !txBuffer_ &&        // No async_write is in progress
               !txQueue_.empty();   // One or more messages are enqueued
    }

    void receive()
    {
        if (socket_)
        {
            rxBuffer_ = getBuffer();
            auto self = this->shared_from_this();
            boost::asio::async_read(*socket_, rxBuffer_->headerBuffer(),
                [this, self](AsioErrorCode ec, size_t)
                {
                    if (check(ec))
                        processHeader();
                });
        }
    }

    virtual void processHeader()
    {
        auto hdr = rxBuffer_->header();
        auto length  = hdr.length();
        if ( check(length <= maxRxLength_, TransportErrc::badRxLength) &&
             check(hdr.msgTypeIsValid(), RawsockErrc::badMessageType) )
        {
            receivePayload(hdr.msgType(), length);
        }
    }

    void receivePayload(RawsockMsgType msgType, size_t length)
    {
        rxBuffer_->resize(length);
        auto self = this->shared_from_this();
        boost::asio::async_read(*socket_, rxBuffer_->payloadBuffer(),
            [this, self, msgType](AsioErrorCode ec, size_t)
            {
                if (ec)
                    rxBuffer_->clear();
                if (check(ec))
                    switch (msgType)
                    {
                    case RawsockMsgType::wamp:
                        if (rxHandler_)
                            post(std::bind(rxHandler_, std::move(rxBuffer_)));
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
        auto buf = getBuffer();
        buf->payload_.swap(rxBuffer_->payload_);
        sendMessage(RawsockMsgType::pong, std::move(buf));
        receive();
    }

    void receivePong()
    {
        if (pingHandler_ && (rxBuffer_->payload_ == pingBuffer_->payload_))
        {
            pingBuffer_.reset();
            namespace chrn = std::chrono;
            pingStop_ = chrn::high_resolution_clock::now();
            using Fms = chrn::duration<float, chrn::milliseconds::period>;
            float elapsed = Fms(pingStop_ - pingStart_).count();
            post(std::bind(pingHandler_, elapsed));
            pingHandler_ = nullptr;
        }
        receive();
    }

    bool check(AsioErrorCode asioEc)
    {
        if (asioEc)
        {
            if (failHandler_)
            {
                auto ec = make_error_code(
                            static_cast<std::errc>(asioEc.value()));
                post(std::bind(failHandler_, ec));
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
                post(std::bind(failHandler_, make_error_code(errc)));
            cleanup();
        }
        return condition;
    }

    void cleanup()
    {
        rxHandler_ = nullptr;
        pingHandler_ = nullptr;
        failHandler_ = nullptr;
        txQueue_ = TransmitQueue();
        socket_.reset();
        rxBuffer_.reset();
        pingBuffer_.reset();
    }

    std::unique_ptr<TSocket> socket_;
    Executor executor_;
    size_t maxTxLength_;
    size_t maxRxLength_;
    bool started_ = false;
    RxHandler rxHandler_;
    FailHandler failHandler_;
    PingHandler pingHandler_;
    Buffer rxBuffer_;
    Buffer pingBuffer_;
    TransmitQueue txQueue_;
    Buffer txBuffer_;
    TimePoint pingStart_;
    TimePoint pingStop_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_ASIOTRANSPORT_HPP
