/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERSESSION_HPP
#define CPPWAMP_INTERNAL_ROUTERSESSION_HPP

#include <atomic>
#include <cstring>
#include <memory>
#include "../accesslogging.hpp"
#include "../authinfo.hpp"
#include "../features.hpp"
#include "../errorinfo.hpp"
#include "../pubsubinfo.hpp"
#include "../rpcinfo.hpp"
#include "../sessioninfo.hpp"
#include "commandinfo.hpp"
#include "random.hpp"
#include "routercontext.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RouterSession
{
public:
    using Ptr = std::shared_ptr<RouterSession>;
    using WeakPtr = std::weak_ptr<RouterSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    virtual ~RouterSession() {}

    SessionId wampId() const {return wampId_.get();}

    const AuthInfo& authInfo() const {return *authInfo_;}

    AuthInfo::Ptr sharedAuthInfo() const {return authInfo_;}

    ClientFeatures features() const {return features_;}

    void setWampId(ReservedId&& id)
    {
        wampId_ = std::move(id);
        sessionInfo_.wampSessionId = wampId();
    }

    void report(AccessActionInfo&& action)
    {
        if (!logger_)
            return;
        logger_->log(AccessLogEntry{transportInfo_, sessionInfo_,
                                    std::move(action)});
    }

    void abort(Reason r) {onRouterAbort(std::move(r));}

    template <typename C, typename... Ts>
    void sendRouterCommand(C&& command, Ts&&... accessInfoArgs)
    {
        if (logger_)
        {
            auto info = command.info(std::forward<Ts>(accessInfoArgs)...);
            logger_->log(AccessLogEntry{transportInfo_, sessionInfo_,
                                        std::move(info)});
        }

        onRouterCommand(std::forward<C>(command));
    }

    void sendEvent(const Event& e)
    {
        // server-event actions are not logged due to the potential large
        // number of observers. Instead, a recipient count is added to the
        // server-published action log.
        onRouterCommand(Event{e});
    }

    RequestId sendInvocation(Invocation&& inv, Uri&& topic)
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        auto id = ++nextOutboundRequestId_;
        assert(id <= 9007199254740992u);
        inv.setRequestId({}, id);
        sendRouterCommand(std::move(inv), std::move(topic));
        return id;
    }

protected:
    RouterSession(RouterLogger::Ptr logger = nullptr)
        : logger_(std::move(logger)),
          authInfo_(std::make_shared<AuthInfo>()),
          nextOutboundRequestId_(0)
    {}

    virtual void onRouterAbort(Reason&&) = 0;

    virtual void onRouterCommand(Error&&) = 0;

    virtual void onRouterCommand(Subscribed&&) = 0;

    virtual void onRouterCommand(Unsubscribed&&) = 0;

    virtual void onRouterCommand(Published&&) = 0;

    virtual void onRouterCommand(Event&&) = 0;

    virtual void onRouterCommand(Registered&&) = 0;

    virtual void onRouterCommand(Unregistered&&) = 0;

    virtual void onRouterCommand(Invocation&&) = 0;

    virtual void onRouterCommand(Result&&) = 0;

    virtual void onRouterCommand(Interruption&&) = 0;

    void setRouterLogger(RouterLogger::Ptr logger)
    {
        logger_ = std::move(logger);
    }

    LogLevel routerLogLevel() const
    {
        return logger_ ? logger_->level() : LogLevel::off;
    }

    void routerLog(LogEntry e)
    {
        if (!logger_)
            return;
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void setTransportInfo(AccessTransportInfo&& info)
    {
        logSuffix_ = " [Session " + info.serverName + '/' +
                     std::to_string(info.serverSessionIndex) + ']';
        transportInfo_ = std::move(info);
    }

    void setHelloInfo(const Realm& hello)
    {
        sessionInfo_.agent = hello.agent().value_or("");
        sessionInfo_.authId = hello.authId().value_or("");
        features_ = hello.features();
    }

    void setWelcomeInfo(AuthInfo&& info)
    {
        // sessionInfo_.wampSessionId was already set
        // via RouterSession::setWampId
        sessionInfo_.realmUri = info.realmUri();
        sessionInfo_.authId = info.id();
        *authInfo_ = std::move(info);
    }

    void resetSessionInfo()
    {
        sessionInfo_.reset();
        wampId_.reset();
        authInfo_->clear();
        features_.reset();
        nextOutboundRequestId_.store(0);
    }

private:
    AccessTransportInfo transportInfo_;
    AccessSessionInfo sessionInfo_;
    String logSuffix_;
    ReservedId wampId_;
    RouterLogger::Ptr logger_;
    AuthInfo::Ptr authInfo_;
    ClientFeatures features_;
    std::atomic<RequestId> nextOutboundRequestId_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERSESSION_HPP
