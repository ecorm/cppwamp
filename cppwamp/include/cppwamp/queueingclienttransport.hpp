/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_QUEUEINGCLIENTTRANSPORT_HPP
#define CPPWAMP_QUEUEINGCLIENTTRANSPORT_HPP

#include <memory>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "transport.hpp"
#include "utils/transportqueue.hpp"
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
    - `void observeHeartbeats(F&& callback)`
    - `void unobserveHeartbeats()`
    - `void ping(const uint8_t* data, std::size_t size, F&& callback)`
    - `void pong(const uint8_t* data, std::size_t size, F&& callback)`
    - `void write(bool fin, const uint8_t* data, std::size_t size, F&& callback)`
    - `void read(B& buffer, std::size_t limit, F&& callback)`
    - `void shutdown(std::error_code reason, F&& callback)`
    - `void close()` */
//------------------------------------------------------------------------------
template <typename TSettings, typename TStream>
class QueueingClientTransport : public Transporting
{
public:
    using Settings    = TSettings;
    using Stream      = TStream;
    using Ptr         = std::shared_ptr<QueueingClientTransport>;
    using Socket      = typename Stream::Socket;
    using SettingsPtr = std::shared_ptr<Settings>;

    QueueingClientTransport(Socket&& socket, SettingsPtr settings,
                            TransportInfo ti)
        : Base(boost::asio::make_strand(socket.get_executor()),
               Stream::makeConnectionInfo(socket),
               ti),
          queue_(makeQueue(std::move(socket), settings, ti)),
          settings_(std::move(settings))
    {
        auto interval = settings_->heartbeatInterval();
        if (internal::timeoutIsDefinite(interval))
        {
            pinger_ = std::make_shared<Pinger>(
                Base::strand(), Base::info().transportId(), interval);
        }
    }

    const Settings& settings() const {return *settings_;}

private:
    using Base = Transporting;
    using Bouncer = utils::AsyncTimerBouncer;
    using Queue = utils::TransportQueue<Stream, Bouncer>;
    using Pinger = internal::Pinger;
    using PingBytes = internal::PingBytes;
    using PingByte = typename PingBytes::value_type;

    static typename Queue::Ptr makeQueue(Socket&& socket,
                                         const SettingsPtr& settings,
                                         const TransportInfo& ti)
    {
        return std::make_shared<Queue>(
            Stream{std::move(socket), settings},
            ti.sendLimit(),
            Bouncer{socket.get_executor(), settings->limits().lingerTimeout()});
    }

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        queue_->start(std::move(rxHandler), std::move(txErrorHandler));
        if (pinger_)
            startPinging();
    }

    void onSend(MessageBuffer message) override
    {
        queue_->send(std::move(message));
    }

    void onAbort(MessageBuffer message, ShutdownHandler handler) override
    {
        halt();
        queue_->abort(std::move(message), std::move(handler));
    }

    void onShutdown(std::error_code reason, ShutdownHandler handler) override
    {
        halt();
        queue_->shutdown(reason, std::move(handler));
    }

    void onClose() override
    {
        halt();
        queue_->close();
    }

    void startPinging()
    {
        std::weak_ptr<Transporting> self = shared_from_this();

        queue_->stream().observeHeartbeats(
            [self, this](TransportFrameKind kind, const PingByte* data,
                         std::size_t size)
            {
                auto me = self.lock();
                if (me)
                    onHeartbeat(kind, data, size);
            });

        pinger_->start(
            [self, this](ErrorOr<PingBytes> pingBytes)
            {
                auto me = self.lock();
                if (me)
                    onPingGeneratedOrTimedOut(pingBytes);
            });
    }

    void onHeartbeat(TransportFrameKind kind, const PingByte* data,
                     std::size_t size)
    {
        if (kind == TransportFrameKind::pong)
        {
            if (pinger_)
                pinger_->pong(data, size);
        }
        else if (kind == TransportFrameKind::ping)
        {
            queue_->send(MessageBuffer{data, data + size},
                         TransportFrameKind::pong);
        }
    }

    void halt()
    {
        if (pinger_)
            pinger_->stop();
        queue_->stream().unobserveHeartbeats();
    }

    void onPingGeneratedOrTimedOut(ErrorOr<PingBytes> pingBytes)
    {
        if (state() != TransportState::running)
            return;

        if (!pingBytes.has_value())
        {
            halt();
            queue_->fail(pingBytes.error());
            return;
        }

        MessageBuffer payload{pingBytes->begin(), pingBytes->end()};
        queue_->send(payload, TransportFrameKind::ping);
    }

    std::shared_ptr<Queue> queue_;
    SettingsPtr settings_;
    std::shared_ptr<Pinger> pinger_;
};

} // namespace wamp

#endif // CPPWAMP_QUEUEINGCLIENTTRANSPORT_HPP
