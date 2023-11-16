/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_QUEUEINGSTREAM_HPP
#define CPPWAMP_INTERNAL_QUEUEINGSTREAM_HPP

#include <deque>
#include <memory>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "../errorcodes.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
#include "transportframe.hpp"
#include "pinger.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TSettings, typename TStream>
class QueueingStream
    : public std::enable_shared_from_this<QueueingStream<TSettings, TStream>>
{
public:
    using Ptr             = std::shared_ptr<QueueingStream>;
    using Settings        = TSettings;
    using Stream          = TStream;
    using Socket          = typename Stream::Socket;
    using SettingsPtr     = std::shared_ptr<Settings>;
    using RxHandler       = typename Transporting::RxHandler;
    using TxErrorHandler  = typename Transporting::TxErrorHandler;
    using ShutdownHandler = typename Transporting::ShutdownHandler;


    QueueingStream(Socket&& socket, SettingsPtr settings, std::size_t sendLimit,
                   Pinger::Ptr pinger = nullptr)
        : timer_(socket.get_executor()),
          stream_(std::move(socket), settings),
          settings_(std::move(settings)),
          pinger_(std::move(pinger)),
          sendLimit_(sendLimit)
    {}

    const Settings& settings() const {return *settings_;}

    void start(RxHandler rxHandler, TxErrorHandler txErrorHandler)
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;
        isRunning_ = true;
        startHeartbearts();
        receive();
    }

    void send(MessageBuffer message)
    {
        if (!stream_.isOpen())
            return;
        auto buf = enframe(std::move(message));
        enqueueFrame(std::move(buf));
    }

    void abort(MessageBuffer message, ShutdownHandler handler) override
    {
        isRunning_ = false;
        if (!stream_.isOpen())
            return;
        auto frame = enframe(std::move(message));
        assert((frame.payload().size() <= sendLimit_) &&
               "Outgoing message is longer than allowed by peer");
        frame.poison();
        shutdownHandler_ = std::move(handler);
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void shutdown(std::error_code reason, ShutdownHandler handler) override
    {
        isRunning_ = false;
        stop(reason, std::move(handler));
    }

    void close() override
    {
        isRunning_ = false;
        halt();
        stream_.close();
    }

private:
    using Frame     = TransportFrame;
    using Byte      = MessageBuffer::value_type;
    using PingBytes = PingBytes;

    void startHeartbearts()
    {
        std::weak_ptr<QueueingStream> self = this->shared_from_this();

        stream_.observeHeartbeats(
            [self, this](TransportFrameKind kind, const Byte* data,
                         std::size_t size)
            {
                auto me = self.lock();
                if (me)
                    onHeartbeat(kind, data, size);
            });

        if (!pinger_)
            return;

        pinger_->start(
            [self, this](ErrorOr<PingBytes> pingBytes)
            {
                auto me = self.lock();
                if (me)
                    onPingGeneratedOrTimedOut(pingBytes);
            });
    }

    void onHeartbeat(TransportFrameKind kind, const Byte* data,
                     std::size_t size)
    {
        if (kind == TransportFrameKind::pong)
        {
            if (pinger_)
                pinger_->pong(data, size);
        }
        else if (kind == TransportFrameKind::ping)
        {
            auto buf = enframe(MessageBuffer{data, data + size},
                               TransportFrameKind::pong);
            enqueueFrame(std::move(buf));
        }
    }

    void stop(std::error_code reason, ShutdownHandler handler)
    {
        if (shutdownHandler_ != nullptr || !stream_.isOpen())
        {
            post(std::move(handler),
                 make_error_code(MiscErrc::invalidState));
            return;
        }

        shutdownHandler_ = std::move(handler);
        halt();
        shutdownTransport(reason);
    }

    void halt()
    {
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        if (pinger_)
            pinger_->stop();
        stream_.unobserveHeartbeats();
    }

    void shutdownTransport(std::error_code reason)
    {
        auto self = this->shared_from_this();
        auto lingerTimeout = settings_->limits().lingerTimeout();

        if (timeoutIsDefinite(lingerTimeout))
        {
            timer_.expires_after(lingerTimeout);
            timer_.async_wait(
                [this, self](boost::system::error_code ec)
                {
                    if (ec == boost::asio::error::operation_aborted)
                        return;
                    onLingerTimeout();
                });
        }

        stream_.shutdown(
            reason,
            [this, self](std::error_code ec, bool flush)
            {
                // When 'flush' is true, successful shutdown is signalled by
                // stream_.readSome emitting TransportErrc::ended
                if (ec || !flush)
                    notifyShutdown(ec);
            });
    }

    void onLingerTimeout()
    {
        stream_.close();
        notifyShutdown(make_error_code(TransportErrc::timeout));
    }

    void onPingGeneratedOrTimedOut(ErrorOr<PingBytes> pingBytes)
    {
        if (!isRunning_)
            return;

        if (!pingBytes.has_value())
            return fail(pingBytes.error());

        MessageBuffer message{pingBytes->begin(), pingBytes->end()};
        auto buf = enframe(std::move(message), TransportFrameKind::ping);
        enqueueFrame(std::move(buf));
    }

    Frame enframe(MessageBuffer&& payload,
                  TransportFrameKind kind = TransportFrameKind::wamp)
    {
        return Frame{std::move(payload), kind};
    }

    void enqueueFrame(Frame&& frame)
    {
        assert((frame.payload().size() <= sendLimit_) &&
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
            });
    }

    void onWampMessageBytesWritten(std::size_t bytesWritten)
    {
        assert(bytesWritten <= txBytesRemaining_);
        txBytesRemaining_ -= bytesWritten;
        if (txBytesRemaining_ > 0)
            return sendMoreWamp();

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
        receiveMore();
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
                if (checkRxError(ec))
                    onRead(n, done);
            });
    }

    void onRead(std::size_t bytesReceived, bool done)
    {
        if (!done)
            return receiveMore();

        if (rxHandler_)
            post(rxHandler_, std::move(rxBuffer_));

        receive();
    }

    bool checkRxError(std::error_code ec)
    {
        if (!ec)
            return true;

        if (ec == TransportErrc::ended)
        {
            bool isShuttingDown = shutdownHandler_ != nullptr;
            if (isShuttingDown)
                notifyShutdown({});
        }

        fail(ec);
        return false;
    }

    void fail(std::error_code ec)
    {
        halt();
        if (rxHandler_)
            post(rxHandler_, makeUnexpected(ec));
        rxHandler_ = nullptr;
    }

    void notifyShutdown(std::error_code ec)
    {
        post(std::move(shutdownHandler_), ec);
        shutdownHandler_ = nullptr;
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(exec_, std::forward<F>(handler), std::forward<Ts>(args)...);
    }

    AnyIoExecutor exec_;
    Stream stream_;
    boost::asio::steady_timer timer_;
    std::deque<Frame> txQueue_;
    Frame txFrame_;
    MessageBuffer rxBuffer_;
    SettingsPtr settings_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    ShutdownHandler shutdownHandler_;
    std::shared_ptr<Pinger> pinger_;
    std::size_t sendLimit_ = 0;
    std::size_t txBytesRemaining_ = 0;
    bool isRunning_ = false;
    bool isTransmitting_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_QUEUEINGSTREAM_HPP
