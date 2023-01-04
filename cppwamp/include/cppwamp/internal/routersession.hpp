/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERSESSION_HPP
#define CPPWAMP_INTERNAL_ROUTERSESSION_HPP

#include <memory>
#include "../authinfo.hpp"
#include "../logging.hpp"
#include "../peerdata.hpp"
#include "idgen.hpp"

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

    virtual void close(bool terminate, Reason) = 0;

    virtual void sendEvent(Event&&) = 0;

    virtual void sendInvocation(Invocation&&) = 0;

    virtual void sendError(Error&&) = 0;

    virtual void sendResult(Result&&) = 0;

    virtual void sendInterruption(Interruption&&) = 0;

    virtual void log(LogEntry&& e) = 0;

    virtual void report(AccessActionInfo&& i) = 0;

protected:
    RouterSession() : authInfo_(std::make_shared<AuthInfo>()) {}

    void clearWampId() {wampId_.reset();}

    void setAuthInfo(AuthInfo&& info)
    {
        if (!authInfo_)
            authInfo_ = std::make_shared<AuthInfo>(std::move(info));
        else
            *authInfo_ = std::move(info);
    }

private:
    ReservedId wampId_;
    AuthInfo::Ptr authInfo_;

public:
    // Internal use only
    void setWampId(PassKey, ReservedId&& id) {wampId_ = std::move(id);}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERSESSION_HPP
