/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_BASICTRANSPORT_HPP
#define CPPWAMP_BASICTRANSPORT_HPP

#include <deque>
#include <memory>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "codec.hpp"
#include "errorcodes.hpp"
#include "messagebuffer.hpp"
#include "transport.hpp"
#include "internal/transportframe.hpp"
#include "internal/pinger.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides outbound message queueing and ping/pong handling for client
    transports.

    @tparam TSettings Transport settings type
    @tparam TStream Class wrapping a networking socket with the following member
                    functions:
    - `AnyIoExecutor executor()`
    - `bool isOpen() const`
    - `void observeControlFrames(F&& callback)`
    - `void unobserveControlFrames()`
    - `void ping(const uint8_t* data, std::size_t size, F&& callback)`
    - `void pong(const uint8_t* data, std::size_t size, F&& callback)`
    - `void write(bool fin, const uint8_t* data, std::size_t size, F&& callback)`
    - `void read(B& buffer, std::size_t limit, F&& callback)`
    - `void shutdown(std::error_code reason, F&& callback)`
    - `void close()` */
//------------------------------------------------------------------------------
template <typename TSettings, typename TStream>
class BasicClientTransport : public Transporting
{
public:
    using Settings    = TSettings;
    using Stream      = TStream;
    using Ptr         = std::shared_ptr<BasicClientTransport>;
    using SettingsPtr = std::shared_ptr<Settings>;

    BasicClientTransport(Stream&& stream, SettingsPtr settings,
                         ConnectionInfo ci, TransportInfo ti)
        : Base(boost::asio::make_strand(stream.executor()), ci, ti),
          timer_(stream.executor()),
          settings_(std::move(settings))
    {
        if (internal::timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<Pinger>(Base::strand(), Base::info());
    }

    const Settings& settings() const {return *settings_;}

private:
    using Base = Transporting;
    using Frame = internal::TransportFrame;
    using Byte = MessageBuffer::value_type;
    using Pinger = internal::Pinger;
    using PingBytes = internal::PingBytes;

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;

        if (pinger_)
            startPinging();

        receive();
    }

    void onSend(MessageBuffer message) override
    {
        if (!stream_.isOpen())
            return;
        auto buf = enframe(std::move(message));
        enqueueFrame(std::move(buf));
    }

    void onSendAbort(MessageBuffer message) override
    {
        if (!stream_.isOpen())
            return;
        auto frame = enframe(std::move(message));
        assert((frame.payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        frame.poison();
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void onShutdown(ShutdownHandler handler) override
    {
        stop({}, std::move(handler));
    }

    void onClose() override
    {
        halt();
        stream_.close();
    }

    void startPinging()
    {
        std::weak_ptr<Transporting> self = shared_from_this();

        stream_.observeControlFrames(
            [self, this](TransportFrameKind kind, const Byte* data,
                         std::size_t size)
            {
                auto me = self.lock();
                if (me)
                    onControlFrame(kind, data, size);
            });

        pinger_->start(
            [self, this](ErrorOr<PingBytes> pingBytes)
            {
                auto me = self.lock();
                if (me)
                    onPingGeneratedOrTimedOut(pingBytes);
            });
    }

    void onControlFrame(TransportFrameKind kind, const Byte* data,
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

    void halt()
    {
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        if (pinger_)
            pinger_->stop();
        stream_.unobserveControlFrames();
    }

    void stop(std::error_code reason, ShutdownHandler handler)
    {
        if (shutdownHandler_ != nullptr || !stream_.isOpen())
        {
            Base::post(std::move(handler),
                       makeUnexpected(MiscErrc::invalidState));
            return;
        }

        shutdownHandler_ = std::move(handler);
        halt();
        auto self = this->shared_from_this();

        auto lingerTimeout = settings().lingerTimeout();
        if (internal::timeoutIsDefinite(lingerTimeout))
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
            [this, self](std::error_code ec)
            {
                // Successful shutdown is signalled by stream_.read
                // emitting TransportErrc::ended
                if (ec)
                {
                    Base::post(shutdownHandler_, makeUnexpected(ec));
                    shutdownHandler_ = nullptr;
                }
            });
    }

    void onLingerTimeout()
    {
        stream_.close();
        if (shutdownHandler_)
        {
            Base::post(shutdownHandler_,
                       makeUnexpectedError(TransportErrc::timeout));
            shutdownHandler_ = nullptr;
        }
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
        assert((frame.payload().size() <= info().maxTxLength()) &&
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

        case TransportFrameKind::ping:
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
        txBytesRemaining_ = 0;
        sendMoreWamp();
    }

    void sendMoreWamp()
    {
        auto bytesSent = txFrame_.payload().size() - txBytesRemaining_;
        const auto* data = txFrame_.payload().data() + bytesSent;
        auto self = shared_from_this();

        stream_.write(
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

        if (txFrame_.isPoisoned())
        {
            if (shutdownHandler_ == nullptr)
                return;
            auto self = shared_from_this();
            stop(make_error_code(TransportErrc::ended),
                 [self](std::error_code) {});
        }
        else
            transmit();
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
        if (!stream_.isOpen())
            return;

        rxBuffer_.clear();
        auto self = this->shared_from_this();
        stream_.read(
            rxBuffer_, settings_->limits().bodySizeLimit(),
            [this, self](std::error_code ec, std::size_t n, bool done)
            {
                if (checkRxError(ec))
                    onRead(ec, n, done);
            });
    }

    void receiveMore()
    {
        if (!stream_.isOpen())
            return;

        auto self = this->shared_from_this();
        stream_.read(
            rxBuffer_, settings_->limits().bodySizeLimit(),
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
    }

    bool checkRxError(std::error_code ec)
    {
        if (!ec)
            return true;

        bool isShuttingDown = shutdownHandler_ != nullptr;
        if (isShuttingDown && (ec == TransportErrc::ended))
        {
            Base::post(shutdownHandler_, true);
            shutdownHandler_ = nullptr;
            return true;
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
    std::size_t txBytesRemaining_ = 0;
    bool isTransmitting_ = false;
};

//------------------------------------------------------------------------------
/** Provides outbound message queueing and timeout handling for
    server transports.

    @tparam TSettings Transport settings type
    @tparam TStream Class wrapping a networking socket with the following member
                    functions:
    - `AnyIoExecutor executor()`
    - `bool isOpen() const`
    - `void observeControlFrames(F&& callback)`
    - `void unobserveControlFrames()`
    - `void ping(const uint8_t* data, std::size_t size, F&& callback)`
    - `void pong(const uint8_t* data, std::size_t size, F&& callback)`
    - `void write(bool fin, const uint8_t* data, std::size_t size, F&& callback)`
    - `void read(B& buffer, std::size_t limit, F&& callback)`
    - `void shutdown(std::error_code reason, F&& callback)`
    - `void close()` */
//------------------------------------------------------------------------------
template <typename TSettings, typename TAdmitter>
class BasicServerTransport : public Transporting
{
public:
    using Settings    = TSettings;
    using Admitter    = TAdmitter;
    using Ptr         = std::shared_ptr<BasicServerTransport>;
    using Socket      = typename Admitter::Socket;
    using SettingsPtr = std::shared_ptr<Settings>;

    BasicServerTransport(Socket&& socket, SettingsPtr settings,
                         ConnectionInfo ci, TransportInfo ti = {})
        : Base(boost::asio::make_strand(socket.get_executor()), ci, ti),
          timer_(socket.get_executor()),
          txTimer_(socket.get_executor()),
          settings_(std::move(settings)),
          admitter_(std::make_shared<Admitter>(std::move(socket)))
    {}

    const Settings& settings() const {return *settings_;}

private:
    using Base = Transporting;
    using Stream = typename TAdmitter::Stream;
    using Frame = internal::TransportFrame;
    using Byte = MessageBuffer::value_type;
    using Pinger = internal::Pinger;
    using PingBytes = internal::PingBytes;

    void onAdmit(AdmitHandler handler) override
    {
        assert((admitter_ != nullptr) && "Admit already performed");
        assert(admitHandler_ == nullptr && "Admit already in progress");

        admitHandler_ = std::move(handler);

        auto self =
            std::dynamic_pointer_cast<BasicServerTransport>(shared_from_this());

        auto timeout = settings_->limits().handshakeTimeout();
        if (timeoutIsDefinite(timeout))
        {
            timer_.expires_from_now(timeout);
            timer_.async_wait(
                [this, self](boost::system::error_code ec)
                {
                    if (ec != boost::asio::error::operation_aborted)
                        onAdmitTimeout(ec);
                });
        }

        struct Admitted
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(AdmitResult result)
            {
                self->onAdmissionCompletion(result, handler);
            }
        };

        bool isShedding = Base::state() == TransportState::shedding;
        admitter_->admit(isShedding,
                         Admitted{std::move(handler), std::move(self)});
    }

    void onAdmitTimeout(boost::system::error_code ec)
    {
        if (admitter_)
            admitter_->cancel();

        if (!admitHandler_)
            return;

        if (ec)
            return admitHandler_(AdmitResult::failed(ec, "timer wait"));

        admitHandler_(AdmitResult::rejected(TransportErrc::timeout));
    }

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;

        auto self =
            std::dynamic_pointer_cast<BasicServerTransport>(shared_from_this());

        stream_.observeControlFrames(
            [self, this](TransportFrameKind kind, const Byte* data,
                         std::size_t size)
            {
                auto me = self.lock();
                if (me)
                    onControlFrame(kind, data, size);
            });

        receive();
    }

    void onSend(MessageBuffer message) override
    {
        if (!stream_.isOpen())
            return;
        auto buf = enframe(std::move(message));
        enqueueFrame(std::move(buf));
    }

    void onSendAbort(MessageBuffer message) override
    {
        if (!stream_.isOpen())
            return;
        auto frame = enframe(std::move(message));
        assert((frame.payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        frame.poison();
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void onShutdown(ShutdownHandler handler) override
    {
        stop({}, std::move(handler));
    }

    void onClose() override
    {
        halt();
        stream_.close();
    }

    void onAdmissionCompletion(AdmitResult result, AdmitHandler& handler)
    {
        timer_.cancel();
        stream_ = admitter_->releaseStream();
        Base::setReady(admitter_->transportInfo());
        Base::post(std::move(handler), result);
        admitter_.reset();
    }

    void onControlFrame(TransportFrameKind kind, const Byte* data,
                        std::size_t size)
    {
        if (kind == TransportFrameKind::ping)
        {
            auto buf = enframe(MessageBuffer{data, data + size},
                               TransportFrameKind::pong);
            enqueueFrame(std::move(buf));
        }
    }

    void halt()
    {
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        stream_.unobserveControlFrames();
    }

    void stop(std::error_code reason, ShutdownHandler handler)
    {
        if (shutdownHandler_ != nullptr || !stream_.isOpen())
        {
            Base::post(std::move(handler),
                       makeUnexpected(MiscErrc::invalidState));
            return;
        }

        shutdownHandler_ = std::move(handler);
        halt();
        auto self = this->shared_from_this();

        auto lingerTimeout = settings().lingerTimeout();
        if (internal::timeoutIsDefinite(lingerTimeout))
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
            [this, self](std::error_code ec)
            {
                // Successful shutdown is signalled by stream_.read
                // emitting TransportErrc::ended
                if (ec)
                {
                    Base::post(shutdownHandler_, makeUnexpected(ec));
                    shutdownHandler_ = nullptr;
                }
            });
    }

    void onLingerTimeout()
    {
        stream_.close();
        if (shutdownHandler_)
        {
            Base::post(shutdownHandler_,
                       makeUnexpectedError(TransportErrc::timeout));
            shutdownHandler_ = nullptr;
        }
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
        assert((frame.payload().size() <= info().maxTxLength()) &&
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

        case TransportFrameKind::ping:
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

        stream_.write(
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

        if (txFrame_.isPoisoned())
        {
            if (shutdownHandler_ == nullptr)
                return;
            auto self = shared_from_this();
            stop(make_error_code(TransportErrc::ended),
                 [self](std::error_code) {});
        }
        else
            transmit();
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
        if (!stream_.isOpen())
            return;

        rxBuffer_.clear();
        auto self = this->shared_from_this();
        stream_.read(
            rxBuffer_, settings_->limits().bodySizeLimit(),
            [this, self](std::error_code ec, std::size_t n, bool done)
            {
                if (checkRxError(ec))
                    onRead(ec, n, done);
            });
    }

    void receiveMore()
    {
        if (!stream_.isOpen())
            return;

        auto self = this->shared_from_this();
        stream_.read(
            rxBuffer_, settings_->limits().bodySizeLimit(),
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
    }

    bool checkRxError(std::error_code ec)
    {
        if (!ec)
            return true;

        bool isShuttingDown = shutdownHandler_ != nullptr;
        if (isShuttingDown && (ec == TransportErrc::ended))
        {
            Base::post(shutdownHandler_, true);
            shutdownHandler_ = nullptr;
            return true;
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

    Stream stream_;
    boost::asio::steady_timer timer_;
    boost::asio::steady_timer txTimer_;
    std::deque<Frame> txQueue_;
    Frame txFrame_;
    MessageBuffer rxBuffer_;
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

#endif // CPPWAMP_BASICTRANSPORT_HPP
