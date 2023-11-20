/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_QUEUEINGSERVERTRANSPORT_HPP
#define CPPWAMP_QUEUEINGSERVERTRANSPORT_HPP

#include <deque>
#include <memory>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "codec.hpp"
#include "errorcodes.hpp"
#include "messagebuffer.hpp"
#include "routerlogger.hpp"
#include "transport.hpp"
#include "internal/pinger.hpp"
#include "internal/servertimeoutmonitor.hpp"
#include "internal/transportframe.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides outbound message queueing and timeout handling for
    server transports.

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
    - `void close()` */
//------------------------------------------------------------------------------
template <typename TSettings, typename TAdmitter>
class QueueingServerTransport : public Transporting
{
public:
    using Settings       = TSettings;
    using Admitter       = TAdmitter;
    using Ptr            = std::shared_ptr<QueueingServerTransport>;
    using ListenerSocket = typename Admitter::ListenerSocket;
    using SettingsPtr    = std::shared_ptr<Settings>;

    QueueingServerTransport(ListenerSocket&& socket, SettingsPtr settings,
                            CodecIdSet codecIds, RouterLogger::Ptr)
        : Base(boost::asio::make_strand(socket.get_executor()),
               Stream::makeConnectionInfo(socket)),
          stream_(socket.get_executor()),
          timer_(socket.get_executor()),
          monitor_(settings),
          settings_(std::move(settings)),
          admitter_(std::make_shared<Admitter>(std::move(socket),
                                               settings_, std::move(codecIds)))
    {}

    template <typename TRequest>
    void upgrade(const TRequest& request, AdmitHandler handler)
    {
        assert((admitter_ != nullptr) && "Admit already performed");
        admitHandler_ = std::move(handler);
        auto self = shared_from_this();
        admitter_->upgrade(
            request,
            [this, self](AdmitResult result) {onAdmissionCompletion(result);});
    }

    const Settings& settings() const {return *settings_;}

protected:
    void onAdmit(AdmitHandler handler) override
    {
        assert((admitter_ != nullptr) && "Admit already performed");
        assert(admitHandler_ == nullptr && "Admit already in progress");

        admitHandler_ = std::move(handler);
        auto self = shared_from_this();
        auto timeout = settings_->limits().handshakeTimeout();

        if (internal::timeoutIsDefinite(timeout))
        {
            std::weak_ptr<Transporting> weakSelf{self};
            timer_.expires_from_now(timeout);
            timer_.async_wait(
                [this, weakSelf](boost::system::error_code ec)
                {
                    auto self = weakSelf.lock();
                    if (!self)
                        return;
                    if (ec != boost::asio::error::operation_aborted)
                        onAdmitTimeout(static_cast<std::error_code>(ec));
                });
        }

        bool isShedding = Base::state() == TransportState::shedding;
        admitter_->admit(
            isShedding,
            [this, self](AdmitResult result) {onAdmissionCompletion(result);});
    }

    std::error_code onMonitor() override {return monitor_.check(now());}

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;

        using Self = QueueingServerTransport;
        std::weak_ptr<Self> self =
            std::dynamic_pointer_cast<Self>(shared_from_this());

        stream_.observeHeartbeats(
            [self, this](TransportFrameKind kind, const Byte* data,
                         std::size_t size)
            {
                auto me = self.lock();
                if (me)
                    onHeartbeat(kind, data, size);
            });

        monitor_.start(now());
        receive();
    }

    void onSend(MessageBuffer message) override
    {
        if (!stream_.isOpen())
            return;
        auto buf = enframe(std::move(message));
        enqueueFrame(std::move(buf));
    }

    void onAbort(MessageBuffer message, ShutdownHandler handler) override
    {
        if (!stream_.isOpen())
        {
            return Base::post(std::move(handler),
                              make_error_code(MiscErrc::invalidState));
        }
        auto frame = enframe(std::move(message));
        assert((frame.payload().size() <= info().sendLimit()) &&
               "Outgoing message is longer than allowed by peer");
        frame.poison();
        shutdownHandler_ = std::move(handler);
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void onShutdown(std::error_code reason, ShutdownHandler handler) override
    {
        struct AdmitShutdown
        {
            ShutdownHandler handler;

            void operator()(std::error_code ec, bool /*flush*/)
            {
                handler(ec);
            }
        };

        if (admitter_ != nullptr)
            admitter_->shutdown(reason, AdmitShutdown{std::move(handler)});
        else
            stop(reason, std::move(handler));
    }

    void onClose() override
    {
        halt();
        if (admitter_ != nullptr)
            admitter_->close();
        else
            stream_.close();
    }

private:
    using Base = Transporting;
    using Stream = typename TAdmitter::Stream;
    using Frame = internal::TransportFrame;
    using Byte = MessageBuffer::value_type;
    using Pinger = internal::Pinger;
    using PingBytes = internal::PingBytes;

    static std::chrono::steady_clock::time_point now()
    {
        return std::chrono::steady_clock::now();
    }

    void onAdmitTimeout(boost::system::error_code ec)
    {
        if (admitter_)
            admitter_->close();

        if (!admitHandler_)
            return;

        if (ec)
            admitHandler_(AdmitResult::failed(ec, "timer wait"));
        else
            admitHandler_(AdmitResult::rejected(TransportErrc::handshakeTimeout));
        admitHandler_ = nullptr;
    }

    void onAdmissionCompletion(AdmitResult result)
    {
        if (admitHandler_ != nullptr)
        {
            if (result.status() == AdmitStatus::wamp)
            {
                timer_.cancel();
                stream_ = Stream{admitter_->releaseSocket(), settings_};
                Base::setReady(admitter_->transportInfo());
                admitter_.reset();
            }
            else if (result.status() == AdmitStatus::failed)
            {
                timer_.cancel();
                admitter_->close();
                admitter_.reset();
            }
            else
            {
                auto self = shared_from_this();
                admitter_->shutdown(
                    result.error(),
                    [this, self](std::error_code, bool)
                    {
                        timer_.cancel();
                        admitter_->close();
                        admitter_.reset();
                    });
            }
            Base::post(std::move(admitHandler_), result);
            admitHandler_ = nullptr;
        }
    }

    void onHeartbeat(TransportFrameKind kind, const Byte* data,
                     std::size_t size)
    {
        if (kind == TransportFrameKind::ping)
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
            Base::post(std::move(handler),
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
        stream_.unobserveHeartbeats();
    }

    void shutdownTransport(std::error_code reason)
    {
        auto self = this->shared_from_this();
        auto lingerTimeout = settings_->limits().lingerTimeout();

        if (internal::timeoutIsDefinite(lingerTimeout))
        {
            std::weak_ptr<Transporting> weakSelf{self};
            timer_.expires_after(lingerTimeout);
            timer_.async_wait(
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

        stream_.shutdown(
            reason,
            [this, self](std::error_code ec, bool flush)
            {
                // When 'flush' is true, successful shutdown is signalled by
                // stream_.readSome emitting TransportErrc::ended
                if (ec || !flush)
                {
                    timer_.cancel();
                    notifyShutdown(ec);
                }
            });
    }

    void onLingerTimeout()
    {
        stream_.close();
        notifyShutdown(make_error_code(TransportErrc::lingerTimeout));
    }

    void onPingGeneratedOrTimedOut(ErrorOr<PingBytes> pingBytes)
    {
        if (state() != TransportState::running)
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
        assert((frame.payload().size() <= info().sendLimit()) &&
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
        monitor_.startWrite(now());

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
        auto self = shared_from_this();

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
        monitor_.updateWrite(now(), bytesWritten);

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
                monitor_.endWrite(now());
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
                monitor_.endWrite(now());
                isTransmitting_ = false;
                if (checkTxError(ec))
                    transmit();
            });
    }

    bool checkTxError(std::error_code ec)
    {
        if (!ec)
            return true;
        monitor_.endWrite(now());
        isTransmitting_ = false;
        if (txErrorHandler_)
            post(txErrorHandler_, ec);
        halt();
        return false;
    }

    void receive()
    {
        rxBuffer_.clear();
        monitor_.startRead(now());
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
        monitor_.updateRead(now(), bytesReceived);

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

        monitor_.endRead(now());

        if (shutdownHandler_ != nullptr)
        {
            timer_.cancel();
            if (ec == TransportErrc::ended)
                notifyShutdown({});
        }

        fail(ec);
        return false;
    }

    void fail(std::error_code ec)
    {
        halt();
        if (rxHandler_)
            Base::post(rxHandler_, makeUnexpected(ec));
        rxHandler_ = nullptr;
    }

    void notifyShutdown(std::error_code ec)
    {
        if (!shutdownHandler_)
            return;
        Base::post(std::move(shutdownHandler_), ec);
        shutdownHandler_ = nullptr;
    }

    Stream stream_; // TODO: Use std::optional<Stream>
    boost::asio::steady_timer timer_;
    std::deque<Frame> txQueue_;
    Frame txFrame_;
    MessageBuffer rxBuffer_;
    internal::ServerTimeoutMonitor<Settings> monitor_;
    SettingsPtr settings_;
    std::shared_ptr<Admitter> admitter_;
    AdmitHandler admitHandler_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    ShutdownHandler shutdownHandler_;
    std::size_t txBytesRemaining_ = 0;
    bool isTransmitting_ = false;
};

} // namespace wamp

#endif // CPPWAMP_QUEUEINGSERVERTRANSPORT_HPP
