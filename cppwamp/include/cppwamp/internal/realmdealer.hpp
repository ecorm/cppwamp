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
class DealerJob
{
public:
    // TODO: Timeouts
    // TODO: Per-registration pending call limits

    DealerJob(const DealerInvocation& inv, const DealerRegistration&,
              RouterSession::Ptr caller, RouterSession::Ptr callee,
              RequestId calleeRequestId)
        : caller_(caller),
          callee_(callee),
          callerKey_(caller->wampId(), inv.callerRequestId()),
          calleeKey_(callee->wampId(), calleeRequestId)
    {}

    ErrorOrDone cancel(CallCancelMode mode, bool& eraseNow)
    {
        auto callee = this->callee_.lock();
        if (!callee)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        mode = callee->features().calleeCancelling ? mode
                                                   : CallCancelMode::skip;

        if (mode != CallCancelMode::skip)
            callee->sendInterruption({{}, calleeKey_.second, mode});

        if (mode == CallCancelMode::killNoWait)
            eraseNow = true;

        if (mode != CallCancelMode::kill)
        {
            discardResultOrError_ = true;
            return makeUnexpectedError(SessionErrc::cancelled);
        }

        return true;
    }

    void complete(Result&& result)
    {
        auto caller = this->caller_.lock();
        if (!caller || discardResultOrError_)
            return;
        result.setRequestId({}, callerKey_.second);
        result.withOptions({});
        caller->sendResult(std::move(result));
    }

    void complete(Error&& error)
    {
        auto caller = this->caller_.lock();
        if (!caller || discardResultOrError_)
            return;
        error.setRequestId({}, callerKey_.second);
        caller->sendError(std::move(error));
    }

private:
    RouterSession::WeakPtr caller_;
    RouterSession::WeakPtr callee_;
    DealerJobKey callerKey_;
    DealerJobKey calleeKey_;
    bool discardResultOrError_ = false;

    friend class DealerJobMap;
};

//------------------------------------------------------------------------------
class DealerJobMap
{
private:
    using ByCaller = std::map<DealerJobKey, DealerJob>;
    using ByCallee = std::map<DealerJobKey, ByCaller::iterator>;

public:
    using Key = DealerJobKey;
    using Job = DealerJob;
    using ByCalleeIterator = ByCallee::iterator;
    using ByCallerIterator = ByCaller::iterator;

    void insert(Job&& job)
    {
        auto callerKey = job.callerKey_;
        auto calleeKey = job.calleeKey_;
        auto emplaced1 = byCaller_.emplace(callerKey, std::move(job));
        assert(emplaced1.second);
        auto emplaced2 = byCallee_.emplace(calleeKey, emplaced1.first);
        assert(emplaced2.second);
    }

    ByCalleeIterator byCalleeFind(Key key) {return byCallee_.find(key);}

    ByCalleeIterator byCalleeEnd() {return byCallee_.end();}

    void byCalleeErase(ByCalleeIterator iter)
    {
        auto callerIter = iter->second;
        byCaller_.erase(callerIter);
        byCallee_.erase(iter);
    }

    ByCallerIterator byCallerFind(Key key) {return byCaller_.find(key);}

    ByCallerIterator byCallerEnd() {return byCaller_.end();}

    void byCallerErase(ByCallerIterator iter)
    {
        auto calleeKey = iter->second.calleeKey_;
        byCallee_.erase(calleeKey);
        byCaller_.erase(iter);
    }

private:
    ByCallee byCallee_;
    ByCaller byCaller_;
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
        // Consensus on what to do with pending invocations upon unregister
        // appears to be to allow them to continue.
        // https://github.com/wamp-proto/wamp-proto/issues/283#issuecomment-429542748
        if (!registry_.erase(callee->wampId(), rid))
            return makeUnexpectedError(SessionErrc::noSuchRegistration);
        return true;
    }

    ErrorOrDone call(RouterSession::Ptr caller, Rpc&& rpc)
    {
        // TODO: Progressive calls
        auto reg = registry_.find(rpc.procedure());
        if (reg == nullptr)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);
        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        DealerInvocation inv(std::move(rpc), reg->registrationId());
        auto calleeReqId = inv.sendTo(*callee);
        jobs_.insert({inv, *reg, caller, callee, calleeReqId});
        return true;
    }

    ErrorOrDone cancelCall(RouterSession::Ptr caller, CallCancellation&& cncl)
    {
        DealerJobKey callerKey{caller->wampId(), cncl.requestId()};
        auto iter = jobs_.byCallerFind(callerKey);
        if (iter == jobs_.byCallerEnd())
            return false;
        using CM = CallCancelMode;
        auto mode = (cncl.mode() == CM::unknown) ? CM::killNoWait : cncl.mode();
        auto& job = iter->second;
        bool eraseNow = false;
        auto done = job.cancel(mode, eraseNow);
        if (eraseNow)
            jobs_.byCallerErase(iter);
        return done;
    }

    void yieldResult(RouterSession::Ptr callee, Result&& result)
    {
        // TODO: Progressive results
        DealerJobKey calleeKey{callee->wampId(), result.requestId()};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        job.complete(std::move(result));
        jobs_.byCalleeErase(iter);
    }

    void yieldError(RouterSession::Ptr callee, Error&& error)
    {
        DealerJobKey calleeKey{callee->wampId(), error.requestId()};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        job.complete(std::move(error));
        jobs_.byCalleeErase(iter);
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
