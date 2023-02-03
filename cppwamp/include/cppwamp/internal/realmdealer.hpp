/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_REALMDEALER_HPP
#define CPPWAMP_INTERNAL_REALMDEALER_HPP

#include <cassert>
#include <map>
#include <tuple>
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
    DealerInvocation(Rpc&& rpc, RegistrationId regId, RouterSession::Ptr)
        : invocation_(invocationFromRpc(std::move(rpc), regId))
    {}

    void sendTo(RouterSession& session)
    {
        session.sendInvocation(std::move(invocation_));
    }

private:
    static Invocation invocationFromRpc(Rpc&& rpc, RegistrationId regId)
    {
        Invocation inv;
        // TODO Invocation inv{rpc.requestId(), regId};
        if (!rpc.args().empty() || !rpc.kwargs().empty())
            inv.withArgList(std::move(rpc).args());
        if (!rpc.kwargs().empty())
            inv.withKwargs(std::move(rpc).kwargs());
        return inv;
    }

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

    bool calleeExpired() const {return callee_.expired();}

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
        auto reg = registry_.find(rpc.procedure());
        if (reg == nullptr || reg->calleeExpired())
            return makeUnexpectedError(SessionErrc::noSuchProcedure);
        DealerInvocation inv(std::move(rpc), reg->registrationId(),
                             std::move(caller));
        reg->invoke(inv);
        // TODO: Pending request map and timeouts
        return true;
    }

    bool cancel(RequestId rid, SessionId sid)
    {
        // TODO
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
    RegistrationId nextRegistrationId()
    {
        RegistrationId id = nextRegistrationId_ + 1;
        while ((id == nullId()) || registry_.contains(id))
            ++id;
        nextRegistrationId_ = id;
        return id;
    }

    DealerRegistry registry_;
    RegistrationId nextRegistrationId_ = nullId();
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REALMDEALER_HPP
