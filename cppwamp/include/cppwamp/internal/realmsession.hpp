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
#include "../authinfo.hpp"
#include "../logging.hpp"
#include "../peerdata.hpp"
#include "random.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct ClientFeatures
{
    static ClientFeatures local()
    {
        ClientFeatures f;
        f.callee           = true;
        f.calleeCancelling = true;
        f.callee           = true;
        f.callerCancelling = true;
        f.publisher        = true;
        f.subscriber       = true;
        return f;
    }

    ClientFeatures()
    {
        std::memset(this, 0, sizeof(ClientFeatures));
    }

    ClientFeatures(const Object& dict)
    {
        parseCalleeFeatures(dict);
        parseCallerFeatures(dict);
        parsePublisherFeatures(dict);
        parseSubscriberFeatures(dict);
    }

    bool callee           : 1;
    bool calleeCancelling : 1;
    bool caller           : 1;
    bool callerCancelling : 1;
    bool publisher        : 1;
    bool subscriber       : 1;

private:
    static bool has(const Object* roleDict, const char* featureName)
    {
        return roleDict->count(featureName) != 0;
    }

    void parseCalleeFeatures(const Object& dict)
    {
        auto d = getRoleDict(dict, "callee");
        if (!d)
            return;
        callee = true;
        calleeCancelling = has(d, "call_cancelling");
    }

    void parseCallerFeatures(const Object& dict)
    {
        auto d = getRoleDict(dict, "caller");
        if (!d)
            return;
        caller = true;
        callerCancelling = has(d, "call_cancelling");
    }

    void parsePublisherFeatures(const Object& dict)
    {
        auto d = getRoleDict(dict, "publisher");
        if (!d)
            return;
        publisher = true;
    }

    void parseSubscriberFeatures(const Object& dict)
    {
        auto d = getRoleDict(dict, "subscriber");
        if (!d)
            return;
        subscriber = true;
    }

    const Object* getRoleDict(const Object& dict, const char* roleName)
    {
        auto found = dict.find(roleName);
        if (found == dict.end() || !found->second.is<Object>())
            return nullptr;
        return &(found->second.as<Object>());
    }
};

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

    virtual void close(bool terminate, Reason) = 0;

    virtual void sendError(Error&&, bool logOnly = false) = 0;

    void sendError(WampMsgType reqType, RequestId rid, std::error_code ec,
                   bool logOnly = false)
    {
        sendError(Error{{}, reqType, rid, ec}, logOnly);
    }

    void sendError(WampMsgType reqType, RequestId rid, SessionErrc errc,
                   bool logOnly = false)
    {
        sendError(reqType, rid, make_error_code(errc), logOnly);
    }

    template <typename T>
    void sendError(WampMsgType reqType, RequestId rid, const ErrorOr<T>& x,
                   bool logOnly = false)
    {
        assert(!x.has_value());
        sendError(reqType, rid, x.error(), logOnly);
    }

    virtual void sendSubscribed(RequestId, SubscriptionId) = 0;

    virtual void sendUnsubscribed(RequestId, String topic) = 0;

    virtual void sendPublished(RequestId, PublicationId) = 0;

    virtual void sendEvent(Event&&, String topic) = 0;

    virtual void sendRegistered(RequestId, RegistrationId) = 0;

    virtual void sendUnregistered(RequestId, String procedure) = 0;

    RequestId sendInvocation(Invocation&& inv)
    {
        // Will take 285 years to reach 2^53 at 1 million requests/sec
        auto id = ++nextOutboundRequestId_;
        assert(id <= 9007199254740992u);
        inv.setRequestId({}, id);
        onSendInvocation(std::move(inv));
        return id;
    }

    virtual void sendResult(Result&&) = 0;

    virtual void sendInterruption(Interruption&&) = 0;

    virtual void log(LogEntry e) = 0;

    virtual void report(AccessActionInfo i) = 0;

protected:
    RealmSession()
        : authInfo_(std::make_shared<AuthInfo>()),
          nextOutboundRequestId_(0)
    {}

    virtual void onSendInvocation(Invocation&&) = 0;

    void clearWampId() {wampId_.reset();}

    void setAuthInfo(AuthInfo&& info)
    {
        if (!authInfo_)
            authInfo_ = std::make_shared<AuthInfo>(std::move(info));
        else
            *authInfo_ = std::move(info);
    }

    void setFeatures(ClientFeatures features) {features_ = features;}

private:
    ReservedId wampId_;
    AuthInfo::Ptr authInfo_;
    ClientFeatures features_;
    std::atomic<RequestId> nextOutboundRequestId_;

public:
    // Internal use only
    void setWampId(PassKey, ReservedId&& id) {wampId_ = std::move(id);}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERSESSION_HPP
