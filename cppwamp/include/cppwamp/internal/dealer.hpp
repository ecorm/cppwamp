/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DEALER_HPP
#define CPPWAMP_INTERNAL_DEALER_HPP

#include <cassert>
#include <chrono>
#include <functional>
#include <map>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "../asiodefs.hpp"
#include "../erroror.hpp"
#include "../uri.hpp"
#include "realmsession.hpp"
#include "timeoutscheduler.hpp"

// TODO: Progressive Calls
// TODO: Progressive Call Results
// TODO: Pending call limits

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class DealerRegistration
{
public:
    static ErrorOr<DealerRegistration> create(Procedure&& procedure,
                                              RealmSession::Ptr callee)
    {
        std::error_code ec;
        DealerRegistration reg(std::move(procedure), callee, ec);
        if (ec)
            return makeUnexpected(ec);
        return reg;
    }

    DealerRegistration() = default;

    void setRegistrationId(RegistrationId rid) {regId_ = rid;}

    const String& procedureUri() const {return procedureUri_;}

    RegistrationId registrationId() const {return regId_;}

    RealmSession::WeakPtr callee() const {return callee_;}

    SessionId calleeId() const {return calleeId_;}

private:
    DealerRegistration(Procedure&& procedure, RealmSession::Ptr callee,
                       std::error_code& ec)
        : procedureUri_(std::move(procedure).uri({})),
          callee_(callee),
          calleeId_(callee->wampId())
    {
        if (procedure.optionByKey("match") != null)
            ec = make_error_code(WampErrc::optionNotAllowed);
    }

    String procedureUri_;
    RealmSession::WeakPtr callee_;
    SessionId calleeId_;
    RegistrationId regId_ = nullId();
};

//------------------------------------------------------------------------------
class DealerRegistry
{
public:
    using Key = std::pair<SessionId, RegistrationId>;

    bool contains(Key key) {return byKey_.count(key) != 0;}

    bool contains(const String& uri) {return byUri_.count(uri) != 0;}

    void insert(Key key, DealerRegistration&& reg)
    {
        reg.setRegistrationId(key.second);
        auto uri = reg.procedureUri();
        auto emplaced = byKey_.emplace(key, std::move(reg));
        assert(emplaced.second);
        auto ptr = &(emplaced.first->second);
        byUri_.emplace(std::move(uri), ptr);
    }

    ErrorOr<String> erase(const Key& key)
    {
        auto found = byKey_.find(key);
        if (found == byKey_.end())
            return makeUnexpectedError(WampErrc::noSuchRegistration);
        auto uri = found->second.procedureUri();
        auto erased = byUri_.erase(uri);
        assert(erased == 1);
        byKey_.erase(found);
        return uri;
    }

    DealerRegistration* find(const String& procedureUri)
    {
        auto found = byUri_.find(procedureUri);
        if (found == byUri_.end())
            return nullptr;
        return found->second;
    }

    void removeCallee(SessionId sessionId)
    {
        auto iter1 = byUri_.begin();
        auto end1 = byUri_.end();
        while (iter1 != end1)
        {
            if (iter1->second->calleeId() == sessionId)
                iter1 = byUri_.erase(iter1);
            else
                ++iter1;
        }

        auto iter2 = byKey_.begin();
        auto end2 = byKey_.end();
        while (iter2 != end2)
        {
            if (iter2->second.calleeId() == sessionId)
                iter2 = byKey_.erase(iter2);
            else
                ++iter2;
        }
    }

private:
    std::map<Key, DealerRegistration> byKey_;
    std::map<String, DealerRegistration*> byUri_;
};

//------------------------------------------------------------------------------
using DealerJobKey = std::pair<SessionId, RequestId>;

//------------------------------------------------------------------------------
class DealerJob
{
public:
    using Timeout = std::chrono::steady_clock::duration;

    static ErrorOr<DealerJob> create(
        RealmSession::Ptr caller, RealmSession::Ptr callee, Rpc&& rpc,
        const DealerRegistration& reg, Invocation& inv)
    {
        DealerJob job{caller, callee, rpc.requestId({})};

        auto timeout = rpc.dealerTimeout();
        job.hasTimeout_ = timeout.has_value() && (*timeout != Timeout{0});
        if (job.hasTimeout_)
            job.timeout_ = *timeout;

        bool callerDisclosed = rpc.discloseMe();

        inv = Invocation({}, std::move(rpc), reg.registrationId());

        if (callerDisclosed)
        {
            // https://github.com/wamp-proto/wamp-proto/issues/57
            const auto& authInfo = caller->authInfo();
            inv.withOption("caller", authInfo.sessionId());
            if (!authInfo.id().empty())
                inv.withOption("caller_authid", authInfo.id());
            if (!authInfo.role().empty())
                inv.withOption("caller_authrole", authInfo.role());
        }
        return job;
    }

    ErrorOrDone cancel(CallCancelMode mode, WampErrc reason, bool& eraseNow)
    {
        using Mode = CallCancelMode;
        assert(mode != Mode::unknown);

        auto callee = this->callee_.lock();
        if (!callee)
            return false;

        mode = callee->features().calleeCancelling ? mode : Mode::skip;

        // Reject duplicate cancellations, except for killnowait that
        // supercedes kill and skip cancellations in progress.
        if (cancelMode_ != Mode::unknown)
        {
            if (mode != Mode::killNoWait || cancelMode_ == Mode::killNoWait)
                return false;
        }

        cancelMode_ = mode;

        if (mode != Mode::skip)
        {
            if (!interruptionSent_)
                callee->sendInterruption({{}, calleeKey_.second, mode, reason});
            interruptionSent_ = true;
        }

        if (mode == Mode::killNoWait)
            eraseNow = true;

        if (mode != Mode::kill)
        {
            discardResultOrError_ = true;
            return makeUnexpectedError(WampErrc::cancelled);
        }

        return true;
    }

    void notifyAbandonedCaller()
    {
        if (discardResultOrError_)
            return;
        auto caller = caller_.lock();
        if (!caller)
            return;

        auto reqId = callerKey_.second;
        auto ec = make_error_code(WampErrc::cancelled);
        auto e = Error({}, WampMsgType::call, reqId, ec)
                     .withArgs("Callee left realm");
        caller->sendError(std::move(e));
    }

    void notifyAbandonedCallee()
    {
        if (interruptionSent_)
            return;
        auto callee = callee_.lock();
        if (!callee)
            return;

        auto reqId = calleeKey_.second;
        if (callee->features().calleeCancelling)
        {
            callee->sendInterruption({{}, reqId, CallCancelMode::killNoWait,
                                      WampErrc::cancelled});
        }
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

    DealerJobKey calleeKey() const {return calleeKey_;}

private:
    DealerJob(const RealmSession::Ptr& caller, const RealmSession::Ptr& callee,
              RequestId callerRequestId)
        : caller_(caller),
          callee_(callee),
          callerKey_(caller->wampId(), callerRequestId),
          calleeKey_(callee->wampId(), nullId())
    {}

    RealmSession::WeakPtr caller_;
    RealmSession::WeakPtr callee_;
    DealerJobKey callerKey_;
    DealerJobKey calleeKey_;
    Timeout timeout_;
    CallCancelMode cancelMode_ = CallCancelMode::unknown;
    bool hasTimeout_ = false;
    bool discardResultOrError_ = false;
    bool interruptionSent_ = false;

    friend class DealerJobMap;
};

//------------------------------------------------------------------------------
class DealerJobMap : public std::enable_shared_from_this<DealerJobMap>
{
private:
    using ByCaller = std::map<DealerJobKey, DealerJob>;
    using ByCallee = std::map<DealerJobKey, ByCaller::iterator>;

public:
    using Key = DealerJobKey;
    using Job = DealerJob;
    using ByCalleeIterator = ByCallee::iterator;
    using ByCallerIterator = ByCaller::iterator;

    DealerJobMap(IoStrand strand)
        : timeoutScheduler_(TimeoutScheduler<Key>::create(std::move(strand)))
    {
        timeoutScheduler_->listen([this](Key k){onTimeout(k);});
    }

    void insert(Job&& job)
    {
        if (job.hasTimeout_)
            timeoutScheduler_->insert(job.timeout_, job.calleeKey_);

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
        auto calleeKey = iter->first;
        auto callerIter = iter->second;
        byCaller_.erase(callerIter);
        byCallee_.erase(iter);
        timeoutScheduler_->erase(calleeKey);
    }

    ByCallerIterator byCallerFind(Key key) {return byCaller_.find(key);}

    ByCallerIterator byCallerEnd() {return byCaller_.end();}

    void byCallerErase(ByCallerIterator iter)
    {
        auto calleeKey = iter->second.calleeKey_;
        byCallee_.erase(calleeKey);
        byCaller_.erase(iter);
        timeoutScheduler_->erase(calleeKey);
    }

    void removeSession(SessionId sessionId)
    {
        auto iter = byCallee_.begin();
        auto end = byCallee_.end();
        while (iter != end)
        {
            SessionId calleeSessionId = iter->first.first;
            SessionId callerSessionId = iter->second->first.first;
            bool calleeMatches = calleeSessionId == sessionId;
            bool callerMatches = callerSessionId == sessionId;

            if (calleeMatches || callerMatches)
            {
                auto& job = iter->second->second;

                if (!callerMatches && calleeMatches)
                    job.notifyAbandonedCaller();

                if (callerMatches && !calleeMatches)
                    job.notifyAbandonedCallee();

                byCaller_.erase(iter->second);
                iter = byCallee_.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

private:
    void onTimeout(Key calleeKey)
    {
        auto iter = byCallee_.find(calleeKey);
        if (iter != byCallee_.end())
        {
            auto& job = iter->second->second;
            bool eraseNow = false;
            job.cancel(CallCancelMode::killNoWait, WampErrc::timeout, eraseNow);
            if (eraseNow)
                byCalleeErase(iter);
        }
    }

    ByCallee byCallee_;
    ByCaller byCaller_;
    TimeoutScheduler<Key>::Ptr timeoutScheduler_;
};

//------------------------------------------------------------------------------
class Dealer
{
public:
    Dealer(IoStrand strand, UriValidator uriValidator)
        : jobs_(std::move(strand)),
          uriValidator_(uriValidator)
    {}

    ErrorOr<RegistrationId> enroll(RealmSession::Ptr callee, Procedure&& p)
    {
        if (!uriValidator_(p.uri(), false))
            return makeUnexpectedError(WampErrc::invalidUri);
        if (registry_.contains(p.uri()))
            return makeUnexpectedError(WampErrc::procedureAlreadyExists);
        auto reg = DealerRegistration::create(std::move(p), callee);
        if (!reg)
            return makeUnexpected(reg.error());
        DealerRegistry::Key key{callee->wampId(), nextRegistrationId()};
        registry_.insert(key, std::move(*reg));
        return key.second;
    }

    ErrorOr<String> unregister(RealmSession::Ptr callee, RegistrationId rid)
    {
        // Consensus on what to do with pending invocations upon unregister
        // appears to be to allow them to continue.
        // https://github.com/wamp-proto/wamp-proto/issues/283#issuecomment-429542748
        return registry_.erase({callee->wampId(), rid});
    }

    ErrorOrDone call(RealmSession::Ptr caller, Rpc&& rpc)
    {
        if (!uriValidator_(rpc.uri(), false))
            return makeUnexpectedError(WampErrc::invalidUri);

        auto reg = registry_.find(rpc.uri());
        if (reg == nullptr)
            return makeUnexpectedError(WampErrc::noSuchProcedure);
        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(WampErrc::noSuchProcedure);

        Invocation inv;
        auto job = DealerJob::create(caller, callee, std::move(rpc), *reg, inv);
        if (!job)
            return makeUnexpected(job.error());

        jobs_.insert(std::move(*job));
        callee->sendInvocation(std::move(inv));
        return true;
    }

    ErrorOrDone cancelCall(RealmSession::Ptr caller, CallCancellation&& cncl)
    {
        DealerJobKey callerKey{caller->wampId(), cncl.requestId()};
        auto iter = jobs_.byCallerFind(callerKey);
        if (iter == jobs_.byCallerEnd())
            return false;
        using CM = CallCancelMode;
        auto mode = (cncl.mode() == CM::unknown) ? CM::killNoWait : cncl.mode();
        auto& job = iter->second;
        bool eraseNow = false;
        auto done = job.cancel(mode, WampErrc::cancelled, eraseNow);
        if (eraseNow)
            jobs_.byCallerErase(iter);
        return done;
    }

    void yieldResult(RealmSession::Ptr callee, Result&& result)
    {
        DealerJobKey calleeKey{callee->wampId(), result.requestId({})};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        job.complete(std::move(result));
        jobs_.byCalleeErase(iter);
    }

    void yieldError(RealmSession::Ptr callee, Error&& error)
    {
        DealerJobKey calleeKey{callee->wampId(), error.requestId({})};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        job.complete(std::move(error));
        jobs_.byCalleeErase(iter);
    }

    void removeCallee(SessionId sessionId)
    {
        registry_.removeCallee(sessionId);
        jobs_.removeSession(sessionId);
    }

private:
    RegistrationId nextRegistrationId() {return ++nextRegistrationId_;}

    DealerRegistry registry_;
    DealerJobMap jobs_;
    UriValidator uriValidator_;
    RegistrationId nextRegistrationId_ = nullId();
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DEALER_HPP
