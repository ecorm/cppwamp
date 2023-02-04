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
        : invocation_({}, std::move(rpc), regId)
    {}

    RequestId requestId() const {return invocation_.requestId();}

    void sendTo(RouterSession& session)
    {
        session.sendInvocation(std::move(invocation_));
    }

private:
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

    void invoke(DealerInvocation& inv) const
    {
        auto callee = callee_.lock();
        if (callee)
            inv.sendTo(*callee);
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

    bool erase(RegistrationId rid, SessionId sid)
    {
        auto found = byRegId_.find(rid);
        if (found == byRegId_.end() || !found->second.belongsTo(sid))
            return false;
        const auto& uri = found->second.procedureUri();
        assert(byUri_.erase(uri) == 1);
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
class RealmDealer
{
public:
    ErrorOr<RegistrationId> enroll(Procedure&& p, RouterSession::Ptr callee)
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

    ErrorOrDone unregister(RegistrationId rid, SessionId sid)
    {
        if (!registry_.erase(rid, sid))
            return makeUnexpectedError(SessionErrc::noSuchRegistration);
        return true;
    }

    ErrorOrDone call(Rpc&& rpc, RouterSession::Ptr caller)
    {
        auto reqId = rpc.requestId({});
        if (pendingInvocations_.count(reqId) != 0)
            return makeUnexpectedError(SessionErrc::protocolViolation);
        auto reg = registry_.find(rpc.procedure());
        if (reg == nullptr)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);
        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        DealerInvocation inv(std::move(rpc), reg->registrationId());
        InvocationRequest req{inv, *reg, caller};
        pendingInvocations_.emplace(reqId, std::move(req));
        reg->invoke(inv);
        return true;
    }

    ErrorOrDone cancelCall(CallCancellation&& cncl, SessionId sid)
    {
        auto reqId = cncl.requestId();
        auto found = pendingInvocations_.find(reqId);
        if (found == pendingInvocations_.end())
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        InvocationRequest& req = found->second;
        auto caller = req.caller.lock();
        auto callee = req.callee.lock();
        if (!callee || !caller || caller->wampId() != sid)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        req.cancelled = true;
        using CM = CallCancelMode;
        auto mode = cncl.mode() == CM::unknown ? CM::killNoWait : cncl.mode();
        mode = callee->features().calleeCancelling ? mode : CM::skip;

        if (mode != CM::skip)
            callee->sendInterruption({{}, reqId, mode});
        if (mode == CM::killNoWait)
            pendingInvocations_.erase(found);
        if (mode != CM::kill)
            return makeUnexpectedError(SessionErrc::cancelled);

        return true;
    }

    void yieldResult(Result&& r, SessionId sid)
    {
        // TODO
    }

    void yieldError(Error&& e, SessionId sid)
    {
        // TODO
    }

private:
    struct InvocationRequest
    {
        // TODO: Timeouts
        // TODO: Per-registration pending call limits

        InvocationRequest(const DealerInvocation& inv,
                          const DealerRegistration& reg,
                          RouterSession::WeakPtr caller)
            : id(inv.requestId()),
              callee(reg.callee()),
              caller(caller)
        {}

        RequestId id;
        RouterSession::WeakPtr callee;
        RouterSession::WeakPtr caller;
        bool cancelled = false;
    };

    RegistrationId nextRegistrationId()
    {
        RegistrationId id = nextRegistrationId_ + 1;
        while ((id == nullId()) || registry_.contains(id))
            ++id;
        nextRegistrationId_ = id;
        return id;
    }

    DealerRegistry registry_;
    std::map<RequestId, InvocationRequest> pendingInvocations_;
    RegistrationId nextRegistrationId_ = nullId();
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REALMDEALER_HPP
