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
#include "routersession.hpp"

// TODO: Progressive Calls
// TODO: Progressive Call Results
// TODO: Pending call limits
// TODO: Interrupt reason: https://github.com/wamp-proto/wamp-proto/issues/156

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class DealerRegistration
{
public:
    static ErrorOr<DealerRegistration> create(Procedure&& procedure,
                                              RouterSession::Ptr callee)
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

    RouterSession::WeakPtr callee() const {return callee_;}

    SessionId calleeId() const {return calleeId_;}

private:
    DealerRegistration(Procedure&& procedure, RouterSession::Ptr callee,
                       std::error_code& ec)
        : procedureUri_(std::move(procedure).uri()),
          callee_(callee),
          calleeId_(callee->wampId())
    {
        if (procedure.optionByKey("match") != null)
            ec = make_error_code(SessionErrc::optionNotAllowed);
    }

    String procedureUri_;
    RouterSession::WeakPtr callee_;
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
            return makeUnexpectedError(SessionErrc::noSuchRegistration);
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
    using Deadline = std::chrono::steady_clock::time_point;

    static ErrorOr<DealerJob> create(
        RouterSession::Ptr caller, RouterSession::Ptr callee, Rpc&& rpc,
        const DealerRegistration& reg, Invocation& inv)
    {
        DealerJob job{caller, callee, rpc.requestId({})};

        auto timeout = rpc.dealerTimeout();
        if (timeout && (*timeout != Deadline::duration{0}))
        {
            // TODO: Prevent overflow
            job.deadline_ = std::chrono::steady_clock::now() + *timeout;
            job.hasDeadline_ = true;
        }

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
                inv.withOption("caller_authrole", authInfo.role());        }
        return job;
    }

    void setCalleeRequestId(RequestId id) {callerKey_.second = id;}

    ErrorOrDone cancel(CallCancelMode mode, bool& eraseNow)
    {
        auto callee = this->callee_.lock();
        if (!callee)
            return false;

        mode = callee->features().calleeCancelling ? mode
                                                   : CallCancelMode::skip;

        if (mode != CallCancelMode::skip)
        {
            callee->sendInterruption({{}, calleeKey_.second, mode});
            interruptionSent_ = true;
        }

        if (mode == CallCancelMode::killNoWait)
            eraseNow = true;

        if (mode != CallCancelMode::kill)
        {
            discardResultOrError_ = true;
            return makeUnexpectedError(SessionErrc::cancelled);
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
        auto ec = make_error_code(SessionErrc::cancelled);
        auto e = Error({}, WampMsgType::call, reqId, ec)
                     .withHint("Callee left realm");
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
            callee->sendInterruption({{}, reqId, CallCancelMode::killNoWait});
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

    bool hasDeadline() const {return hasDeadline_;}

    Deadline deadline() const {return deadline_;}

private:
    DealerJob(const RouterSession::Ptr& caller,
              const RouterSession::Ptr& callee,
              RequestId callerRequestId)
        : caller_(caller),
          callee_(callee),
          callerKey_(caller->wampId(), callerRequestId),
          calleeKey_(callee->wampId(), nullId())
    {}

    RouterSession::WeakPtr caller_;
    RouterSession::WeakPtr callee_;
    DealerJobKey callerKey_;
    DealerJobKey calleeKey_;
    Deadline deadline_;
    bool hasDeadline_ = false;
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

    DealerJobMap(IoStrand strand) : timer_(strand) {}

    void insert(Job&& job)
    {
        updateTimeoutForInserted(job);
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
        updateTimeoutForErased(calleeKey);
    }

    ByCallerIterator byCallerFind(Key key) {return byCaller_.find(key);}

    ByCallerIterator byCallerEnd() {return byCaller_.end();}

    void byCallerErase(ByCallerIterator iter)
    {
        auto calleeKey = iter->second.calleeKey_;
        byCallee_.erase(calleeKey);
        byCaller_.erase(iter);
        updateTimeoutForErased(calleeKey);
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
    using Deadline = Job::Deadline;

    void updateTimeoutForInserted(const Job& newJob)
    {
        if (newJob.hasDeadline() && newJob.deadline() < nextDeadline_)
            startTimer(newJob.calleeKey(), newJob.deadline());
    }

    void updateTimeoutForErased(Key erasedCalleeKey)
    {
        if (timeoutCalleeKey_ == erasedCalleeKey)
            if (!armNextTimeout())
                timer_.cancel();
    }

    void startTimer(Key key, Deadline deadline)
    {
        timeoutCalleeKey_ = key;
        nextDeadline_ = deadline;
        std::weak_ptr<DealerJobMap> self{shared_from_this()};
        timer_.expires_at(deadline);
        timer_.async_wait(
            [this, self, key](boost::system::error_code ec)
            {
                auto me = self.lock();
                if (me)
                {
                    nextDeadline_ = Deadline::max();
                    if (ec)
                        onTimeout(key);
                    else
                        assert(ec == boost::asio::error::operation_aborted);
                }
            });
    }

    void onTimeout(DealerJobKey calleeKey)
    {
        auto iter = byCallee_.find(calleeKey);
        if (iter != byCallee_.end())
        {
            auto& job = iter->second->second;
            bool eraseNow = false;
            job.cancel(CallCancelMode::killNoWait, eraseNow);
            if (eraseNow)
                byCalleeErase(iter);
        }
        armNextTimeout();
    }

    bool armNextTimeout()
    {
        Key earliest;
        auto deadline = Deadline::max();
        bool found = false;

        for (const auto& kv: byCallee_)
        {
            const auto& job = kv.second->second;
            if (job.hasDeadline() && job.deadline() < deadline)
            {
                earliest = kv.first;
                deadline = job.deadline();
                found = true;
            }
        }

        if (found)
            startTimer(earliest, deadline);
        return found;
    }

    boost::asio::steady_timer timer_;
    ByCallee byCallee_;
    ByCaller byCaller_;
    Key timeoutCalleeKey_;
    Deadline nextDeadline_ = Deadline::max();
};

//------------------------------------------------------------------------------
class Dealer
{
public:
    Dealer(IoStrand strand, UriValidator uriValidator)
        : jobs_(std::move(strand)),
          uriValidator_(uriValidator)
    {}

    ErrorOr<RegistrationId> enroll(RouterSession::Ptr callee, Procedure&& p)
    {
        if (!uriValidator_(p.uri(), false))
            return makeUnexpectedError(SessionErrc::invalidUri);
        if (registry_.contains(p.uri()))
            return makeUnexpectedError(SessionErrc::procedureAlreadyExists);
        auto reg = DealerRegistration::create(std::move(p), callee);
        if (!reg)
            return makeUnexpected(reg.error());
        DealerRegistry::Key key{callee->wampId(), nextRegistrationId()};
        registry_.insert(key, std::move(*reg));
        return key.second;
    }

    ErrorOr<String> unregister(RouterSession::Ptr callee, RegistrationId rid)
    {
        // Consensus on what to do with pending invocations upon unregister
        // appears to be to allow them to continue.
        // https://github.com/wamp-proto/wamp-proto/issues/283#issuecomment-429542748
        return registry_.erase({callee->wampId(), rid});
    }

    ErrorOrDone call(RouterSession::Ptr caller, Rpc&& rpc)
    {
        if (!uriValidator_(rpc.uri(), false))
            return makeUnexpectedError(SessionErrc::invalidUri);

        auto reg = registry_.find(rpc.uri());
        if (reg == nullptr)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);
        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(SessionErrc::noSuchProcedure);

        Invocation inv;
        auto job = DealerJob::create(caller, callee, std::move(rpc), *reg, inv);
        if (!job)
            return makeUnexpected(job.error());

        jobs_.insert(std::move(*job));
        callee->sendInvocation(std::move(inv));
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
        DealerJobKey calleeKey{callee->wampId(), result.requestId({})};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        job.complete(std::move(result));
        jobs_.byCalleeErase(iter);
    }

    void yieldError(RouterSession::Ptr callee, Error&& error)
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
