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

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RealmSession
{
public:
    using Ptr = std::shared_ptr<RealmSession>;
    using WeakPtr = std::weak_ptr<RealmSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    virtual ~RealmSession() {}

    SessionId wampId() const {return wampId_.get();}

    const AuthInfo& authInfo() const {return *authInfo_;}

    AuthInfo::Ptr sharedAuthInfo() const {return authInfo_;}

    ClientFeatures features() const {return features_;}

    void setWampId(ReservedId&& id)
    {
        wampId_ = std::move(id);
        sessionInfo_.wampSessionId = wampId();
    }

    template <typename TLogger>
    void report(AccessActionInfo&& action, TLogger& logger)
    {
        logger.log(AccessLogEntry{transportInfo_, sessionInfo_,
                                  std::move(action)});
    }

    virtual void abort(Reason) = 0;

    virtual void sendError(Error&&, bool logOnly = false) = 0;

    void sendError(MessageKind reqKind, RequestId rid, std::error_code ec,
                   bool logOnly = false)
    {
        sendError(Error{{}, reqKind, rid, ec}, logOnly);
    }

    void sendError(MessageKind reqKind, RequestId rid, WampErrc errc,
                   bool logOnly = false)
    {
        sendError(reqKind, rid, make_error_code(errc), logOnly);
    }

    template <typename T>
    void sendError(MessageKind reqKind, RequestId rid, const ErrorOr<T>& x,
                   bool logOnly = false)
    {
        assert(!x.has_value());
        sendError(reqKind, rid, x.error(), logOnly);
    }

    virtual void sendSubscribed(Subscribed&&) = 0;

    virtual void sendUnsubscribed(Unsubscribed&&, Uri&& topic) = 0;

    virtual void sendPublished(Published&&) = 0;

    virtual void sendEvent(Event&&, Uri topic) = 0;

    virtual void sendRegistered(Registered&&) = 0;

    virtual void sendUnregistered(Unregistered&&, Uri&& procedure) = 0;

    RequestId sendInvocation(Invocation&& inv)
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        auto id = ++nextOutboundRequestId_;
        assert(id <= 9007199254740992u);
        inv.setRequestId({}, id);
        onSendInvocation(std::move(inv));
        return id;
    }

    virtual void sendResult(Result&&) = 0;

    virtual void sendInterruption(Interruption&&) = 0;

protected:
    RealmSession()
        : authInfo_(std::make_shared<AuthInfo>()),
          nextOutboundRequestId_(0)
    {}

    virtual void onSendInvocation(Invocation&&) = 0;

    void setTransportInfo(AccessTransportInfo&& info)
    {
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
        // via RealmSession::setWampId
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
    ReservedId wampId_;
    AuthInfo::Ptr authInfo_;
    ClientFeatures features_;
    std::atomic<RequestId> nextOutboundRequestId_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERSESSION_HPP
