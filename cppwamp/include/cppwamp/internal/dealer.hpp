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
#include "../errorcodes.hpp"
#include "../errorinfo.hpp"
#include "../erroror.hpp"
#include "../rpcinfo.hpp"
#include "../uri.hpp"
#include "routersession.hpp"
#include "timeoutscheduler.hpp"

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

    const Uri& procedureUri() const {return procedureUri_;}

    RegistrationId registrationId() const {return regId_;}

    RouterSession::WeakPtr callee() const {return callee_;}

    SessionId calleeId() const {return calleeId_;}

private:
    DealerRegistration(Procedure&& procedure, RouterSession::Ptr callee,
                       std::error_code& ec)
        : procedureUri_(std::move(procedure).uri({})),
          callee_(callee),
          calleeId_(callee->wampId())
    {
        if (procedure.optionByKey("match") != null)
            ec = make_error_code(WampErrc::optionNotAllowed);
    }

    Uri procedureUri_;
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

    bool contains(const Uri& uri) {return byUri_.count(uri) != 0;}

    void insert(Key key, DealerRegistration&& reg)
    {
        reg.setRegistrationId(key.second);
        auto uri = reg.procedureUri();
        auto emplaced = byKey_.emplace(key, std::move(reg));
        assert(emplaced.second);
        auto ptr = &(emplaced.first->second);
        byUri_.emplace(std::move(uri), ptr);
    }

    ErrorOr<Uri> erase(const Key& key)
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

    DealerRegistration* find(const Uri& procedure)
    {
        auto found = byUri_.find(procedure);
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
    std::map<Uri, DealerRegistration*> byUri_;
};

//------------------------------------------------------------------------------
using DealerJobKey = std::pair<SessionId, RequestId>;

//------------------------------------------------------------------------------
class DealerJob
{
public:
    using Timeout = std::chrono::steady_clock::duration;

    static ErrorOr<DealerJob> create(
        RouterSession::Ptr caller, RouterSession::Ptr callee, const Rpc& rpc,
        const DealerRegistration& reg)
    {
        WampErrc errc = WampErrc::success;
        DealerJob job{std::move(caller), std::move(callee), rpc, reg, errc};
        if (errc != WampErrc::success)
            return makeUnexpectedError(errc);
        return job;
    }

    DealerJob() = default;

    Invocation makeInvocation(RouterSession::Ptr caller, Rpc&& rpc) const
    {
        bool callerDisclosed = rpc.discloseMe();
        bool hasTrustLevel = rpc.hasTrustLevel({});
        auto trustLevel = rpc.trustLevel({});

        Invocation inv{{}, std::move(rpc), registrationId_};

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

        if (progressiveResultsRequested_)
            inv.withOption("receive_progress", true);

        if (hasTrustLevel)
            inv.withOption("trust_level", trustLevel);

        return inv;
    }

    ErrorOr<Invocation> makeProgressiveInvocation(Rpc&& rpc)
    {
        if (!isProgressiveCall_)
            return makeUnexpectedError(WampErrc::protocolViolation);
        isProgressiveCall_ = rpc.isProgress({});
        Invocation inv{{}, std::move(rpc), registrationId_};

        // Only propagate the `progress` option. The initial progressive
        // call is what establishes other options for the duration of the
        // progressive call transfer.
        // https://github.com/wamp-proto/wamp-proto/issues/446#issue-1594260780
        if (isProgressiveCall_)
            inv.withOption("progress", true);

        return inv;
    }

    ErrorOrDone cancel(CallCancelMode mode, WampErrc reason, bool& eraseNow)
    {
        using Mode = CallCancelMode;
        assert(mode != Mode::unknown);

        auto callee = this->callee_.lock();
        if (!callee)
            return false;

        bool calleeHasCallCanceling =
            callee->features().callee().all_of(CalleeFeatures::callCanceling);
        mode = calleeHasCallCanceling ? mode : Mode::skip;

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
            {
                Interruption intr{{}, calleeKey_.second, mode, reason};
                callee->sendRouterCommand(std::move(intr));
            }
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
        if (interruptionSent_)
            return;
        auto callee = callee_.lock();
        if (!callee)
            return;

        auto reqId = calleeKey_.second;
        if (callee->features().callee().all_of(CalleeFeatures::callCanceling))
        {
            Interruption intr{{}, reqId, CallCancelMode::killNoWait,
                              WampErrc::cancelled};
            callee->sendRouterCommand(std::move(intr));
        }
    }

    void notifyAbandonedCallee()
    {
        if (discardResultOrError_)
            return;
        auto caller = caller_.lock();
        if (!caller)
            return;

        auto reqId = callerKey_.second;
        auto ec = make_error_code(WampErrc::noAvailableCallee);
        auto e = Error({}, MessageKind::call, reqId, ec)
                     .withArgs("Callee left realm");
        caller->sendRouterCommand(std::move(e), true);
    }

    // Returns true if the job must be erased
    bool yield(Result&& result)
    {
        auto caller = this->caller_.lock();
        if (!caller || discardResultOrError_)
            return true;
        result.setRequestId({}, callerKey_.second);
        bool isProgress = result.optionOr<bool>("progress", false);
        result.withOptions({});
        if (isProgress)
            result.withOption("progress", true);
        caller->sendRouterCommand(std::move(result), true);
        return !progressiveResultsRequested_ || !isProgress;
    }

    void yield(Error&& error)
    {
        auto caller = this->caller_.lock();
        if (!caller || discardResultOrError_)
            return;
        error.setRequestId({}, callerKey_.second);
        caller->sendRouterCommand(std::move(error), true);
    }

    DealerJobKey callerKey() const {return callerKey_;}

    DealerJobKey calleeKey() const {return calleeKey_;}

    bool hasTimeout() const {return hasTimeout_;}

    Timeout timeout() const {return timeout_;}

    bool isProgressiveCall() const {return isProgressiveCall_;}

    bool progressiveResultsRequested() const
    {
        return progressiveResultsRequested_;
    }

private:
    DealerJob(RouterSession::Ptr caller, RouterSession::Ptr callee,
              const Rpc& rpc, const DealerRegistration& reg, WampErrc& errc)
        : caller_(caller),
          callee_(callee),
          callerKey_(caller->wampId(), rpc.requestId({})),
          calleeKey_(callee->wampId(), nullId()),
          registrationId_(reg.registrationId())
    {
        auto timeout = rpc.dealerTimeout();
        hasTimeout_ = timeout.has_value() && (*timeout != Timeout{0});
        if (hasTimeout_)
            timeout_ = *timeout;

        auto calleeFeatures = callee->features().callee();
        bool calleeHasCallCancelling =
            calleeFeatures.all_of(CalleeFeatures::callCanceling);

        // Not clear what the behavior should be when progressive results are
        // requested, but not supported by the callee.
        // https://github.com/wamp-proto/wamp-proto/issues/467
        if (rpc.progressiveResultsAreEnabled({}))
        {
            bool calleeHasProgressiveCallResults =
                calleeHasCallCancelling &&
                calleeFeatures.all_of(CalleeFeatures::progressiveCallResults);
            progressiveResultsRequested_ = calleeHasProgressiveCallResults;
        }

        if (rpc.isProgress({}))
        {
            bool calleeHasProgressiveCallInvocations =
                calleeHasCallCancelling &&
                calleeFeatures.all_of(CalleeFeatures::progressiveCallInvocations);

            if (!calleeHasProgressiveCallInvocations)
            {
                errc = WampErrc::featureNotSupported;
                return;
            }

            isProgressiveCall_ = true;
        }
    }

    RouterSession::WeakPtr caller_;
    RouterSession::WeakPtr callee_;
    DealerJobKey callerKey_;
    DealerJobKey calleeKey_;
    Timeout timeout_ = {};
    RegistrationId registrationId_ = nullId();
    CallCancelMode cancelMode_ = CallCancelMode::unknown;
    bool hasTimeout_ = false;
    bool isProgressiveCall_ = false;
    bool progressiveResultsRequested_ = false;
    bool discardResultOrError_ = false;
    bool interruptionSent_ = false;
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
        if (job.hasTimeout())
            timeoutScheduler_->insert(job.calleeKey(), job.timeout());

        auto callerKey = job.callerKey();
        auto calleeKey = job.calleeKey();
        auto emplaced1 = byCaller_.emplace(callerKey, std::move(job));
        assert(emplaced1.second);
        auto emplaced2 = byCallee_.emplace(calleeKey, emplaced1.first);
        assert(emplaced2.second);
        lastInsertedCallerRequestId_ = callerKey.second;
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
        auto calleeKey = iter->second.calleeKey();
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

    void updateProgressiveResultDeadline(const Job& job)
    {
        if (job.hasTimeout() && job.progressiveResultsRequested())
            timeoutScheduler_->update(job.calleeKey(), job.timeout());
    }

    RequestId lastInsertedCallerRequestId() const
    {
        return lastInsertedCallerRequestId_;
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
    RequestId lastInsertedCallerRequestId_ = nullId();
};

//------------------------------------------------------------------------------
class Dealer
{
public:
    Dealer(IoStrand strand, UriValidator::Ptr uriValidator)
        : jobs_(std::move(strand)),
          uriValidator_(std::move(uriValidator))
    {}

    ErrorOr<RegistrationId> enroll(RouterSession::Ptr callee, Procedure&& p)
    {
        if (!uriValidator_->checkProcedure(p.uri(), false))
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

    ErrorOr<Uri> unregister(RouterSession::Ptr callee, RegistrationId rid)
    {
        // Consensus on what to do with pending invocations upon unregister
        // appears to be to allow them to continue.
        // https://github.com/wamp-proto/wamp-proto/issues/283#issuecomment-429542748
        return registry_.erase({callee->wampId(), rid});
    }

    ErrorOrDone call(RouterSession::Ptr caller, Rpc&& rpc)
    {
        if (!uriValidator_->checkProcedure(rpc.uri(), false))
            return makeUnexpectedError(WampErrc::invalidUri);

        auto reg = registry_.find(rpc.uri());
        if (reg == nullptr)
            return makeUnexpectedError(WampErrc::noSuchProcedure);

        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(WampErrc::noSuchProcedure);

        auto rpcReqId = rpc.requestId({});
        bool isContinuation = rpcReqId <= jobs_.lastInsertedCallerRequestId();
        if (isContinuation)
        {
            return continueCall(std::move(caller), std::move(callee),
                                std::move(rpc), *reg);
        }
        return newCall(std::move(caller), std::move(callee),
                       std::move(rpc), *reg);
    }

    ErrorOrDone newCall(RouterSession::Ptr caller, RouterSession::Ptr callee,
                        Rpc&& rpc, const DealerRegistration& reg)
    {
        auto uri = rpc.uri();
        auto job = DealerJob::create(caller, callee, rpc, reg);
        if (!job)
            return makeUnexpected(job.error());
        auto inv = job->makeInvocation(caller, std::move(rpc));
        jobs_.insert(std::move(*job));
        callee->sendRouterCommand(std::move(inv), std::move(uri));
        return true;
    }

    ErrorOrDone continueCall(RouterSession::Ptr caller,
                             RouterSession::Ptr callee, Rpc&& rpc,
                             const DealerRegistration& reg)
    {
        auto uri = rpc.uri();
        auto found = jobs_.byCallerFind({caller->wampId(), rpc.requestId({})});
        if (found == jobs_.byCallerEnd())
            return makeUnexpectedError(WampErrc::noSuchProcedure);
        auto& job = found->second;
        auto inv = job.makeProgressiveInvocation(std::move(rpc));
        if (!inv)
            return UnexpectedError{inv.error()};
        callee->sendRouterCommand(std::move(*inv), std::move(uri));
        return true;
    }

    ErrorOrDone cancelCall(RouterSession::Ptr caller, CallCancellation&& cncl)
    {
        DealerJobKey callerKey{caller->wampId(), cncl.requestId({})};
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

    void yieldResult(RouterSession::Ptr callee, Result&& result)
    {
        DealerJobKey calleeKey{callee->wampId(), result.requestId({})};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        if (job.yield(std::move(result)))
            jobs_.byCalleeErase(iter);
        else
            jobs_.updateProgressiveResultDeadline(job);
    }

    void yieldError(RouterSession::Ptr callee, Error&& error)
    {
        DealerJobKey calleeKey{callee->wampId(), error.requestId({})};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        job.yield(std::move(error));
        jobs_.byCalleeErase(iter);
    }

    void removeSession(SessionId sessionId)
    {
        registry_.removeCallee(sessionId);
        jobs_.removeSession(sessionId);
    }

private:
    RegistrationId nextRegistrationId() {return ++nextRegistrationId_;}

    DealerRegistry registry_;
    DealerJobMap jobs_;
    UriValidator::Ptr uriValidator_;
    RegistrationId nextRegistrationId_ = nullId();
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DEALER_HPP
