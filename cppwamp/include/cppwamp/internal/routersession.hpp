/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERSESSION_HPP
#define CPPWAMP_INTERNAL_ROUTERSESSION_HPP

#include <memory>
#include "../routerconfig.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
class RouterSession
{
public:
    using Ptr = std::shared_ptr<RouterSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    virtual ~RouterSession() {}

    SessionId wampSessionId() const {return wampSessionId_;}

    const AuthInfo& authInfo() const {return *authInfo_;}

    AuthInfo::Ptr sharedAuthInfo() const {return authInfo_;}

    virtual void terminate(String hint) = 0;

    virtual void shutDown(String hint, String reasonUri) = 0;

    virtual void sendSubscribed(RequestId, SubscriptionId) = 0;

    virtual void sendUnsubscribed(RequestId) = 0;

    virtual void sendRegistered(RequestId, RegistrationId) = 0;

    virtual void sendUnregistered(RequestId) = 0;

    virtual void sendInvocation(Invocation&&) = 0;

    virtual void sendError(Error&&) = 0;

    virtual void sendResult(Result&&) = 0;

    virtual void sendInterruption(Interruption&&) = 0;

    virtual void log(LogEntry&& e) = 0;

    virtual void logAccess(AccessActionInfo&& i) = 0;

protected:
    RouterSession() : authInfo_(std::make_shared<AuthInfo>()) {}

    void clearWampSessionId() {wampSessionId_ = nullId();}

    void setWampSessionId(SessionId id) {wampSessionId_ = id;}

    void setAuthInfo(AuthInfo&& info)
    {
        if (!authInfo_)
            authInfo_ = std::make_shared<AuthInfo>(std::move(info));
        else
            *authInfo_ = std::move(info);
    }

private:
    SessionId wampSessionId_ = nullId();
    AuthInfo::Ptr authInfo_;
};


} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERSESSION_HPP
