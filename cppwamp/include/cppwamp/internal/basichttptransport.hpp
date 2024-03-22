/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_BASICHTTPTRANSPORT_HPP
#define CPPWAMP_INTERNAL_BASICHTTPTRANSPORT_HPP

#include <boost/asio/strand.hpp>
#include "../routerlogger.hpp"
#include "httpjobimpl.hpp"
#include "basicwebsockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TTraits>
class BasicHttpServerTransport : public Transporting
{
public:
    using Ptr            = std::shared_ptr<BasicHttpServerTransport>;
    using HttpSocket     = typename TTraits::Socket;
    using Settings       = typename TTraits::ServerSettings;
    using SettingsPtr    = std::shared_ptr<Settings>;
    using SslContextType = typename TTraits::SslContextType;

    BasicHttpServerTransport(HttpSocket&& t, SettingsPtr s, const CodecIdSet& c,
                             RouterLogger::Ptr l, SslContextType ssl = {})
        : Base(boost::asio::make_strand(t.get_executor()),
               makeConnectionInfo(t)),
          job_(std::make_shared<HttpJobImplType>(std::move(t), std::move(s), c,
                                                 Base::connectionInfo(),
                                                 std::move(l))),
        sslContext_(std::move(ssl))
    {}

private:
    using Base            = Transporting;
    using HttpJobImplType = HttpJobImpl<TTraits>;
    using WsTraits        = typename TTraits::WsTraits;
    using WsTransport     = BasicWebsocketServerTransport<WsTraits>;

    static ConnectionInfo makeConnectionInfo(const HttpSocket& socket)
    {
        return TTraits::makeConnectionInfo(socket);
    }

    static std::error_code
    netErrorCodeToStandard(boost::system::error_code netEc)
    {
        if (!netEc)
            return {};

        namespace AE = boost::asio::error;
        bool disconnected = netEc == AE::broken_pipe ||
                            netEc == AE::connection_reset ||
                            netEc == AE::eof;
        auto ec = disconnected
                      ? make_error_code(TransportErrc::disconnected)
                      : static_cast<std::error_code>(netEc);

        if (netEc == AE::operation_aborted)
            ec = make_error_code(TransportErrc::aborted);

        return ec;
    }

    void onAdmit(AdmitHandler handler) override
    {
        assert((job_ != nullptr) && "HTTP job already performed");

        struct Processed
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(AdmitResult result)
            {
                self->onJobProcessed(result, handler);
            }
        };

        auto self = std::dynamic_pointer_cast<BasicHttpServerTransport>(
            this->shared_from_this());

        bool isShedding = Base::state() == TransportState::shedding;
        job_->process(isShedding,
                      Processed{std::move(handler), std::move(self)});
    }

    std::error_code onMonitor() override
    {
        if (job_)
            return job_->monitor();
        else if (transport_)
            return transport_->monitor();
        return {};
    }

    void onStart(RxHandler r, TxErrorHandler t) override
    {
        assert(transport_ != nullptr);
        transport_->httpStart({}, std::move(r), std::move(t));
    }

    void onSend(MessageBuffer m) override
    {
        assert(transport_ != nullptr);
        transport_->httpSend({}, std::move(m));
    }

    void onAbort(MessageBuffer m, ShutdownHandler f) override
    {
        if (job_ != nullptr)
        {
            job_->shutdown(Base::abortReason(), std::move(f));
            return;
        }

        assert(transport_ != nullptr);
        transport_->httpAbort({}, std::move(m), std::move(f));
    }

    void onShutdown(std::error_code reason, ShutdownHandler f) override
    {
        if (job_ != nullptr)
        {
            job_->shutdown(reason, std::move(f));
            return;
        }

        assert(transport_ != nullptr);
        transport_->httpShutdown({}, reason, std::move(f));
    }

    void onClose() override
    {
        if (job_ != nullptr)
            job_->close();
        else if (transport_ != nullptr)
            transport_->httpClose({});
    }

    void onJobProcessed(AdmitResult result, AdmitHandler& handler)
    {
        if (result.status() == AdmitStatus::wamp)
        {
            transport_ = job_->upgradedTransport();
            job_.reset();
            Base::setReady(transport_->info(),
                           transport_->connectionInfo().target());
        }
        handler(result);
    }

    std::shared_ptr<HttpJobImplType> job_;
    std::shared_ptr<WsTransport> transport_;
    SslContextType sslContext_; // Lifetime management only, not used directly
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BASICHTTPTRANSPORT_HPP
