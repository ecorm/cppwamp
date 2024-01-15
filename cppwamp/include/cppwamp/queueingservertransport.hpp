/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_QUEUEINGSERVERTRANSPORT_HPP
#define CPPWAMP_QUEUEINGSERVERTRANSPORT_HPP

#include <memory>
#include <utility>
#include "codec.hpp"
#include "messagebuffer.hpp"
#include "routerlogger.hpp"
#include "transport.hpp"
#include "utils/transportqueue.hpp"
#include "internal/servertimeoutmonitor.hpp"

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
          monitor_(std::make_shared<Monitor>(settings)),
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
        monitor_->startHandshake(now());

        bool isShedding = Base::state() == TransportState::shedding;
        auto self = shared_from_this();
        admitter_->admit(
            isShedding,
            [this, self](AdmitResult result) {onAdmissionCompletion(result);});
    }

    std::error_code onMonitor() override
    {
        auto tick = now();
        if (queue_)
            queue_->monitor(tick);
        return monitor_->check(tick);
    }

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {

        using Self = QueueingServerTransport;
        std::weak_ptr<Self> self =
            std::dynamic_pointer_cast<Self>(shared_from_this());

        queue_->observeHeartbeats(
            [self, this](TransportFrameKind kind, const Byte* data,
                         std::size_t size)
            {
                auto me = self.lock();
                if (me)
                    onHeartbeat(kind, data, size);
            });

        queue_->start(std::move(rxHandler), std::move(txErrorHandler));
    }

    void onSend(MessageBuffer message) override
    {
        queue_->send(std::move(message));
    }

    void onAbort(MessageBuffer message, ShutdownHandler handler) override
    {
        queue_->abort(std::move(message), std::move(handler));
    }

    void onShutdown(std::error_code reason, ShutdownHandler handler) override
    {
        struct AdmitShutdown
        {
            ShutdownHandler handler;
            Ptr self;

            void operator()(std::error_code ec)
            {
                self->monitor_->endLinger();
                handler(ec);
            }
        };

        if (admitter_ == nullptr)
            return queue_->shutdown(reason, std::move(handler));

        monitor_->startLinger(now());
        auto self = std::dynamic_pointer_cast<QueueingServerTransport>(
            shared_from_this());
        admitter_->shutdown(reason,
                            AdmitShutdown{std::move(handler), std::move(self)});
    }

    void onClose() override
    {
        if (admitter_ != nullptr)
            admitter_->close();
        else if (queue_ != nullptr)
            queue_->close();
    }

private:
    using Base = Transporting;
    using Stream = typename TAdmitter::Stream;
    using Socket = typename Stream::Socket;
    using Bouncer = utils::PollingBouncer;
    using Monitor = internal::ServerTimeoutMonitor<Settings>;
    using Queue = utils::TransportQueue<Stream, Bouncer, Monitor>;
    using Frame = internal::TransportFrame;
    using Byte = MessageBuffer::value_type;

    static typename Queue::Ptr makeQueue(Socket&& socket,
                                         const SettingsPtr& settings,
                                         const TransportInfo& ti,
                                         std::shared_ptr<Monitor> monitor)
    {
        return std::make_shared<Queue>(
            Stream{std::move(socket), settings},
            Bouncer{socket.get_executor(), settings->limits().lingerTimeout()},
            ti.sendLimit(),
            std::move(monitor));
    }

    static std::chrono::steady_clock::time_point now()
    {
        return std::chrono::steady_clock::now();
    }

    void onAdmissionCompletion(AdmitResult result)
    {
        monitor_->endHandshake();

        if (admitHandler_ != nullptr)
        {
            switch (result.status())
            {
            case AdmitStatus::wamp:
                queue_ = makeQueue(admitter_->releaseSocket(),
                                   settings_, admitter_->transportInfo(),
                                   monitor_);
                Base::setReady(admitter_->transportInfo(),
                               admitter_->releaseTargetPath());
                admitter_.reset();
                break;

            case AdmitStatus::rejected:
                Base::setRejected();
                break;

            case AdmitStatus::failed:
                admitter_->close();
                admitter_.reset();
                break;

            default:
                // Do nothing
                break;
            }

            Base::post(std::move(admitHandler_), result);
            admitHandler_ = nullptr;
        }
    }

    void onHeartbeat(TransportFrameKind kind, const Byte* data,
                     std::size_t size)
    {
        monitor_->heartbeat(now());

        if (kind == TransportFrameKind::ping)
        {
            queue_->send(MessageBuffer{data, data + size},
                         TransportFrameKind::pong);
        }
    }

    std::shared_ptr<Queue> queue_;
    std::shared_ptr<Monitor> monitor_;
    SettingsPtr settings_;
    std::shared_ptr<Admitter> admitter_;
    AdmitHandler admitHandler_;
};

} // namespace wamp

#endif // CPPWAMP_QUEUEINGSERVERTRANSPORT_HPP
