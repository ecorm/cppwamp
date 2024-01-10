/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TRANSPORT_QUEUE_HPP
#define CPPWAMP_UTILS_TRANSPORT_QUEUE_HPP

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "../anyhandler.hpp"
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../messagebuffer.hpp"
#include "../timeout.hpp"
#include "../transport.hpp"
#include "../internal/transportframe.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
class AsyncTimerBouncer
{
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    AsyncTimerBouncer(AnyIoExecutor exec, Timeout timeout)
        : timer_(std::move(exec)),
          timeout_(timeout)
    {}

    bool enabled() const {return wamp::internal::timeoutIsDefinite(timeout_);}

    template <typename F>
    void start(F&& callback)
    {
        struct Awaited
        {
            Decay<F> callback;

            void operator()(boost::system::error_code ec)
            {
                if (ec == boost::asio::error::operation_aborted)
                    return;
                callback(static_cast<std::error_code>(ec));
            }
        };

        timer_.expires_after(timeout_);
        timer_.async_wait(Awaited{std::move(callback)});
    }

    void monitor(TimePoint) {}

    void cancel()
    {
        timer_.cancel();
    }

private:
    boost::asio::steady_timer timer_;
    Timeout timeout_;
};

//------------------------------------------------------------------------------
class PollingBouncer
{
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    PollingBouncer(AnyIoExecutor exec, Timeout timeout)
        : executor_(std::move(exec)),
          timeout_(timeout)
    {}

    bool enabled() const {return wamp::internal::timeoutIsDefinite(timeout_);}

    template <typename F>
    void start(F&& callback)
    {
        handler_ = std::forward<F>(callback);
        deadline_ = std::chrono::steady_clock::now() + timeout_;
    }

    void monitor(TimePoint tick)
    {
        if (tick < deadline_)
            return;

        postAny(executor_, std::move(handler_), std::error_code{});
        reset();
    }

    void cancel() {reset();}

private:
    void reset()
    {
        handler_ = nullptr;
        deadline_ = TimePoint::max();
    }

    AnyIoExecutor executor_;
    AnyCompletionHandler<void (std::error_code)> handler_;
    Timeout timeout_;
    TimePoint deadline_ = TimePoint::max();
};

//------------------------------------------------------------------------------
class NullTimeoutMonitor
{
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    void start(TimePoint) {}

    void stop() {}

    void startRead(TimePoint) {}

    void updateRead(TimePoint, std::size_t) {}

    void endRead(TimePoint) {}

    void startWrite(TimePoint, bool) {}

    void updateWrite(TimePoint, std::size_t) {}

    void endWrite(TimePoint, bool) {}

    void heartbeat(TimePoint) {}

    std::error_code check(TimePoint) const {return {};}
};

//------------------------------------------------------------------------------
/** Provides inbound message receiving and outbound message queueing for
    transports.

    @tparam TSettings Transport settings type
    @tparam TStream Class wrapping a networking socket with the following member
                    functions:
    - `AnyIoExecutor executor()`
    - `bool isOpen() const`
    - `void observeHeartbeats(F&& callback)`
    - `void unobserveHeartbeats()`
    - `void ping(const uint8_t* data, std::size_t size, F&& callback)`
    - `void pong(const uint8_t* data, std::size_t size, F&& callback)`
    - `void write(bool fin, const uint8_t* data, std::size_t size, F&& callback)`
    - `void read(B& buffer, std::size_t limit, F&& callback)`
    - `void shutdown(std::error_code reason, F&& callback)`
    - `void close()`
    @tparam TBouncer Class that enforces linger timeout
    @tparam TMonitor Class that enforces other server timeouts. */
//------------------------------------------------------------------------------
template <typename TStream, typename TBouncer,
         typename TMonitor = NullTimeoutMonitor>
class TransportQueue
    : public std::enable_shared_from_this<
          TransportQueue<TStream, TBouncer, TMonitor>>
{
public:
    using Stream          = TStream;
    using Bouncer         = TBouncer;
    using Monitor         = TMonitor;
    using Ptr             = std::shared_ptr<TransportQueue>;
    using Socket          = typename Stream::Socket;
    using RxHandler       = Transporting::RxHandler;
    using TxErrorHandler  = Transporting::TxErrorHandler;
    using ShutdownHandler = Transporting::ShutdownHandler;
    using TimePoint       = std::chrono::steady_clock::time_point;

    TransportQueue(TStream&& stream, Bouncer&& bouncer,
                   std::size_t txPayloadLimit,
                   std::shared_ptr<Monitor> monitor = nullptr)
        : stream_(std::move(stream)),
          bouncer_(std::move(bouncer)),
          monitor_(std::move(monitor)),
          txPayloadLimit_(txPayloadLimit)
    {}

    template <typename F>
    void observeHeartbeats(F&& callback)
    {
        stream_.observeHeartbeats(std::forward<F>(callback));
    }

    const Stream& stream() const {return stream_;}

    void start(RxHandler rxHandler, TxErrorHandler txErrorHandler)
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;
        if (monitor_)
            monitor_->start(now());
        receive();
    }

    void send(MessageBuffer payload,
              TransportFrameKind kind = TransportFrameKind::wamp)
    {
        if (!stream_.isOpen())
            return;
        auto buf = enframe(std::move(payload), kind);
        enqueueFrame(std::move(buf));
    }

    void abort(MessageBuffer message, ShutdownHandler handler)
    {
        assert(stream_.isOpen());
        stream_.unobserveHeartbeats();

        // Start the linger timeout countdown so that a stalled outbound
        // message does not indefinitely prolong the connection lifetime.
        startBouncer();

        auto frame = enframe(std::move(message));
        assert((frame.payload().size() <= txPayloadLimit_) &&
               "Outgoing message is longer than allowed by peer");
        frame.poison();

        shutdownHandler_ = std::move(handler);
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void shutdown(std::error_code reason, ShutdownHandler handler)
    {
        assert(stream_.isOpen());
        stream_.unobserveHeartbeats();
        shutdownHandler_ = std::move(handler);
        halt();
        shutdownTransport(reason);
    }

    void close()
    {
        halt();
        if (monitor_)
            monitor_->stop();
        stream_.unobserveHeartbeats();
        stream_.close();
    }

    void monitor(TimePoint tick)
    {
        bouncer_.monitor(tick);
    }

    void fail(std::error_code ec)
    {
        halt();
        stream_.unobserveHeartbeats();
        if (monitor_)
            monitor_->stop();
        if (rxHandler_)
        {
            auto handler = std::move(rxHandler_);
            rxHandler_ = nullptr;
            handler(makeUnexpected(ec));
        }
    }

private:
    using Base = Transporting;
    using Frame = wamp::internal::TransportFrame;
    using Byte = MessageBuffer::value_type;

    static std::chrono::steady_clock::time_point now()
    {
        return std::chrono::steady_clock::now();
    }

    void halt()
    {
        txErrorHandler_ = nullptr;
        txQueue_.clear();
    }

    void shutdownTransport(std::error_code reason)
    {
        startBouncer();
        auto self = this->shared_from_this();
        stream_.shutdown(
            reason,
            [this, self](std::error_code ec) {notifyShutdown(ec);});
    }

    void startBouncer()
    {
        if (!bouncer_.enabled())
            return;

        std::weak_ptr<TransportQueue> weakSelf = this->shared_from_this();

        bouncer_.start(
            [this, weakSelf](boost::system::error_code ec)
            {
                auto self = weakSelf.lock();
                if (!self)
                    return;
                if (ec == boost::asio::error::operation_aborted)
                    return;
                onLingerTimeout();
            });
    }

    void onLingerTimeout()
    {
        notifyShutdown(make_error_code(TransportErrc::lingerTimeout));
        stream_.close();
    }

    Frame enframe(MessageBuffer&& payload,
                  TransportFrameKind kind = TransportFrameKind::wamp)
    {
        return Frame{std::move(payload), kind};
    }

    void enqueueFrame(Frame&& frame)
    {
        assert((frame.payload().size() <= txPayloadLimit_) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.emplace_back(std::move(frame));
        transmit();
    }

    void transmit()
    {
        if (!isReadyToTransmit())
            return;

        txFrame_ = std::move(txQueue_.front());
        txQueue_.pop_front();
        bool bumpLoiter = txFrame_.kind() == TransportFrameKind::wamp;
        if (monitor_)
            monitor_->startWrite(now(), bumpLoiter);

        switch (txFrame_.kind())
        {
        case TransportFrameKind::wamp:
            return sendWamp();

        case TransportFrameKind::ping:
            return sendPing();

        case TransportFrameKind::pong:
            return sendPong();

        default:
            assert(false && "Unexpected TransportFrameKind enumerator");
            break;
        }
    }

    bool isReadyToTransmit() const
    {
        return stream_.isOpen() && !isTransmitting_ && !txQueue_.empty();
    }

    void sendWamp()
    {
        auto self = this->shared_from_this();
        isTransmitting_ = true;
        txBytesRemaining_ = txFrame_.payload().size();
        sendMoreWamp();
    }

    void sendMoreWamp()
    {
        auto bytesSent = txFrame_.payload().size() - txBytesRemaining_;
        const auto* data = txFrame_.payload().data() + bytesSent;
        auto self = this->shared_from_this();

        stream_.writeSome(
            data, txBytesRemaining_,
            [this, self](std::error_code ec, std::size_t bytesWritten)
            {
                if (checkTxError(ec))
                    onWampMessageBytesWritten(bytesWritten);
                else if (monitor_)
                    monitor_->endWrite(now(), true);
            });
    }

    void onWampMessageBytesWritten(std::size_t bytesWritten)
    {
        if (monitor_)
            monitor_->updateWrite(now(), bytesWritten);

        assert(bytesWritten <= txBytesRemaining_);
        txBytesRemaining_ -= bytesWritten;
        if (txBytesRemaining_ > 0)
            return sendMoreWamp();

        if (monitor_)
            monitor_->endWrite(now(), true);

        isTransmitting_ = false;

        if (!txFrame_.isPoisoned())
            transmit();
        else if (shutdownHandler_ != nullptr)
            shutdownTransport({});
    }

    void sendPing()
    {
        auto self = this->shared_from_this();
        isTransmitting_ = true;
        stream_.ping(
            txFrame_.payload().data(), txFrame_.payload().size(),
            [this, self](std::error_code ec)
            {
                if (monitor_)
                    monitor_->endWrite(now(), false);
                isTransmitting_ = false;
                if (checkTxError(ec))
                    transmit();
            });
    }

    void sendPong()
    {
        auto self = this->shared_from_this();
        isTransmitting_ = true;
        stream_.pong(
            txFrame_.payload().data(), txFrame_.payload().size(),
            [this, self](std::error_code ec)
            {
                if (monitor_)
                    monitor_->endWrite(now(), false);
                isTransmitting_ = false;
                if (checkTxError(ec))
                    transmit();
            });
    }

    bool checkTxError(std::error_code ec)
    {
        if (!ec)
            return true;
        isTransmitting_ = false;
        if (txErrorHandler_)
            post(txErrorHandler_, ec);
        halt();
        return false;
    }

    void receive()
    {
        rxBuffer_.clear();
        auto self = this->shared_from_this();
        stream_.awaitRead(
            rxBuffer_,
            [this, self](std::error_code ec, std::size_t n, bool done)
            {
                if (checkRxError(ec))
                    onReadReady(n, done);
            });
    }

    void onReadReady(std::size_t bytesReceived, bool done)
    {
        if (!stream_.isOpen())
            return;

        if (monitor_)
            monitor_->startRead(now());

        onRead(bytesReceived, done);
    }

    void onRead(std::size_t /*bytesReceived*/, bool done)
    {
        if (!done)
            return receiveMore();

        if (monitor_)
            monitor_->endRead(now());

        if (rxHandler_)
            post(rxHandler_, std::move(rxBuffer_));

        receive();
    }

    void receiveMore()
    {
        if (!stream_.isOpen())
            return;

        auto self = this->shared_from_this();
        stream_.readSome(
            rxBuffer_,
            [this, self](std::error_code ec, std::size_t n, bool done)
            {
                if (!checkRxError(ec))
                    return;
                if (monitor_)
                    monitor_->updateRead(now(), n);
                onRead(n, done);
            });
    }

    bool checkRxError(std::error_code ec)
    {
        if (!ec)
            return true;
        fail(ec);
        return false;
    }

    void notifyShutdown(std::error_code ec)
    {
        bouncer_.cancel();
        if (!shutdownHandler_)
            return;
        auto handler = std::move(shutdownHandler_);
        shutdownHandler_ = nullptr;
        handler(ec);
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(stream_.executor(), std::forward<F>(handler),
                std::forward<Ts>(args)...);
    }

    Stream stream_;
    Bouncer bouncer_;
    std::deque<Frame> txQueue_;
    Frame txFrame_;
    MessageBuffer rxBuffer_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    ShutdownHandler shutdownHandler_;
    std::shared_ptr<Monitor> monitor_;
    std::size_t txPayloadLimit_ = 0;
    std::size_t txBytesRemaining_ = 0;
    bool isTransmitting_ = false;
};

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_UTILS_TRANSPORT_QUEUE_HPP
