/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_REALMDEALER_HPP
#define CPPWAMP_INTERNAL_REALMDEALER_HPP

#include <cassert>
#include <map>
#include <utility>
#include "../erroror.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class DealerInvocation
{
public:
    DealerInvocation(Rpc&& rpc, RegistrationId regId)
        : callerRequestId_(rpc.requestId({})),
          invocation_({}, std::move(rpc), regId)
    {}

    RequestId callerRequestId() const {return callerRequestId_;}

    RequestId sendTo(RouterSession& session)
    {
        return session.sendInvocation(std::move(invocation_));
    }

private:
    RequestId callerRequestId_;
    Invocation invocation_;
};

//------------------------------------------------------------------------------
class DealerRegistration
{
public:
    DealerRegistration() = default;

    DealerRegistration(Procedure&& procedure, RouterSession::WeakPtr callee)
        : procedureUri_(std::move(procedure).uri()),
          callee_(callee)
    {}

    std::error_code check() const
    {
        // TODO: Reject prefix/wildcard matching as unsupported
        // TODO: Check URI validity
        return {};
    }

    void setRegistrationId(RegistrationId rid) {regId_ = rid;}

    const String& procedureUri() const {return procedureUri_;}

    RegistrationId registrationId() const {return regId_;}

    bool belongsTo(SessionId sid) const
    {
        auto callee = callee_.lock();
        return (callee != nullptr) && (callee->wampId() == sid);
    }

    RouterSession::WeakPtr callee() const {return callee_;}

private:
    String procedureUri_;
    RouterSession::WeakPtr callee_;
    RegistrationId regId_ = nullId();
};

//------------------------------------------------------------------------------
class DealerRegistry
{
public:
    bool contains(RegistrationId id) {return byRegId_.count(id) != 0;}

    bool contains(const String& uri) {return byUri_.count(uri) != 0;}

    void insert(DealerRegistration&& reg, RegistrationId id)
    {
        auto uri = reg.procedureUri();
        reg.setRegistrationId(id);
        auto emplaced = byRegId_.emplace(id, std::move(reg));
        assert(emplaced.second);
        auto ptr = &(emplaced.first->second);
        byUri_.emplace(std::move(uri), ptr);
    }

    bool erase(SessionId sid, RegistrationId rid)
    {
        auto found = byRegId_.find(rid);
        if (found == byRegId_.end() || !found->second.belongsTo(sid))
            return false;
        const auto& uri = found->second.procedureUri();
        auto erased = byUri_.erase(uri);
        assert(erased == 1);
        byRegId_.erase(found);
        return true;
    }

    DealerRegistration* find(const String& procedureUri)
    {
        auto found = byUri_.find(procedureUri);
        if (found == byUri_.end())
            return nullptr;
        return found->second;
    }

private:
    std::map<RegistrationId, DealerRegistration> byRegId_;
    std::map<String, DealerRegistration*> byUri_;
};

//------------------------------------------------------------------------------
using DealerJobKey = std::pair<SessionId, RequestId>;

//------------------------------------------------------------------------------
struct DealerJob
{
    // TODO: Timeouts
    // TODO: Per-registration pending call limits

    DealerJob(const DealerInvocation& inv, const DealerRegistration&,
              RouterSession::Ptr caller, RouterSession::Ptr callee,
              RequestId calleeRequestId)
        : caller(caller),
          callee(callee),
          callerKey(caller->wampId(), inv.callerRequestId()),
          calleeKey(callee->wampId(), calleeRequestId)
    {}

    RouterSession::WeakPtr caller;
    RouterSession::WeakPtr callee;
    DealerJobKey callerKey;
    DealerJobKey calleeKey;
    bool cancelled = false;
};

//------------------------------------------------------------------------------
class DealerJobMap
{
public:
    using Key = DealerJobKey;
    using Job = DealerJob;

    void add(Job&& job)
    {
        auto calleeKey = job.calleeKey;
        auto callerKey = job.callerKey;
        auto emplaced = byCallee_.emplace(calleeKey, std::move(job));
        assert(emplaced.second);
        auto ptr = &(emplaced.first->second);
        auto emplaced2 = byCaller_.emplace(callerKey, ptr);
        assert(emplaced2.second);
    }

    void erase(const DealerJob& job)
    {
        byCallee_.erase(job.calleeKey);
        byCaller_.erase(job.callerKey);
    }

    DealerJob* findByCallee(Key key)
    {
        auto found = byCallee_.find(key);
        return (found == byCallee_.end()) ? nullptr : &(found->second);
    }

    DealerJob* findByCaller(Key key)
    {
        auto found = byCaller_.find(key);
        return (found == byCaller_.end()) ? nullptr : found->second;
    }


private:
    std::map<Key, Job> byCallee_;
    std::map<Key, Job*> byCaller_;
};

//------------------------------------------------------------------------------
class RealmDealer
{
public:
    ErrorOr<RegistrationId> enroll(RouterSession::Ptr callee, Procedure&& p)
    {
        if (registry_.contains(p.uri()))
            return makeUnexpectedError(SessionErrc::procedureAlreadyExists);
        DealerRegistration reg{std::move(p), callee};
        auto ec = reg.check();
        if (ec)
            return makeUnexpected(ec);
        auto regId = nextRegistrationId();
        registry_.insert(std::move(reg), regId);
        return regId;
    }

    ErrorOrDone unregister(RouterSession::Ptr callee, RegistrationId rid)
    {
        // TODO: Cancel pending requests that can no longer be fulfilled
        if (!registry_.erase(callee->wampId(), rid))
            return makeUnexpectedError(SessionErrc::noSuchRegistration);
        return true;
    }

    ErrorOrDone call(RouterSession::Ptr caller, Rpc&& rpc)
    {
        // TODO: Check monotonic caller request ID
        // TODO: Progressive calls
        auto reg = registry_.find(rpc.procedure());
        if (reg == nullptr)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);
        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        DealerInvocation inv(std::move(rpc), reg->registrationId());
        auto calleeReqId = inv.sendTo(*callee);
        DealerJob req{inv, *reg, caller, callee, calleeReqId};
        jobs_.add(std::move(req));
        return true;
    }

    ErrorOrDone cancelCall(RouterSession::Ptr caller, CallCancellation&& cncl)
    {
        DealerJobKey callerKey{caller->wampId(), cncl.requestId()};
        auto job = jobs_.findByCaller(callerKey);
        if (!job)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        auto callee = job->callee.lock();
        if (!callee)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        job->cancelled = true;
        using CM = CallCancelMode;
        auto mode = cncl.mode() == CM::unknown ? CM::killNoWait : cncl.mode();
        mode = callee->features().calleeCancelling ? mode : CM::skip;

        if (mode != CM::skip)
            callee->sendInterruption({{}, job->calleeKey.second, mode});
        if (mode == CM::killNoWait)
            jobs_.erase(*job);
        if (mode != CM::kill)
            return makeUnexpectedError(SessionErrc::cancelled);

        return true;
    }

    void yieldResult(RouterSession::Ptr callee, Result&& result)
    {
        // TODO: Progressive results
        DealerJobKey calleeKey{callee->wampId(), result.requestId()};
        auto job = jobs_.findByCallee(calleeKey);
        if (!job)
            return;

        auto caller = job->caller.lock();
        if (!caller)
            return;
        result.setRequestId({}, job->callerKey.second);
        result.withOptions({});
        jobs_.erase(*job);
        caller->sendResult(std::move(result));
    }

    void yieldError(RouterSession::Ptr callee, Error&& error)
    {
        DealerJobKey calleeKey{callee->wampId(), error.requestId()};
        auto job = jobs_.findByCallee(calleeKey);
        if (!job)
            return;

        auto caller = job->caller.lock();
        if (!caller)
            return;
        error.setRequestId({}, job->callerKey.second);
        jobs_.erase(*job);
        caller->sendError(std::move(error));
    }

private:
    RegistrationId nextRegistrationId()
    {
        RegistrationId id = nextRegistrationId_ + 1;
        if (id)
        while ((id == nullId()) || registry_.contains(id))
            ++id;
        nextRegistrationId_ = id;
        return id;
    }

    DealerRegistry registry_;
    DealerJobMap jobs_;
    RegistrationId nextRegistrationId_ = nullId();
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REALMDEALER_HPP
