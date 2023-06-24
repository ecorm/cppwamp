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
#include <mutex>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../errorinfo.hpp"
#include "../erroror.hpp"
#include "../rpcinfo.hpp"
#include "metaapi.hpp"
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
    DealerRegistration(Procedure&& procedure, const RouterSession::Ptr& callee)
        : info_(std::move(procedure).uri({}), MatchPolicy::exact,
                InvocationPolicy::single, 0, std::chrono::system_clock::now()),
          callee_(callee),
          calleeId_(callee->wampId())
    {
        info_.callees.emplace(calleeId_);
        info_.calleeCount = 1;
    }

    DealerRegistration() = default;

    void setRegistrationId(RegistrationId rid) {info_.id = rid;}

    void resetCallee()
    {
        callee_ = {};
        calleeId_ = nullId();
        info_.callees.clear();
        info_.calleeCount = 0;
    }

    const Uri& procedureUri() const & {return info_.uri;}

    Uri&& procedureUri() && {return std::move(info_.uri);} // TODO: Check if legit

    RouterSession::WeakPtr callee() const {return callee_;}

    SessionId calleeId() const {return calleeId_;}

    const RegistrationInfo& info() const {return info_;}

    RegistrationInfo info(bool listCallees) const
    {
        if (listCallees)
            return info_;

        RegistrationInfo r{info_.uri, info_.matchPolicy, info_.invocationPolicy,
                           info_.id, info_.created};
        r.calleeCount = info_.calleeCount;
        return r;
    }

private:
    RegistrationInfo info_;
    RouterSession::WeakPtr callee_;
    SessionId calleeId_ = nullId();
};

//------------------------------------------------------------------------------
class DealerRegistry
{
public:
    using Key = RegistrationId;

    bool contains(Key key) {return byKey_.count(key) != 0;}

    bool contains(const Uri& uri) {return byUri_.count(uri) != 0;}

    DealerRegistration& insert(Key key, DealerRegistration&& reg)
    {
        reg.setRegistrationId(key);
        auto uri = reg.procedureUri();
        auto emplaced = byKey_.emplace(key, std::move(reg));
        assert(emplaced.second);
        auto* ptr = &(emplaced.first->second);
        byUri_.emplace(std::move(uri), ptr);
        return emplaced.first->second;
    }

    ErrorOr<Uri> erase(RouterSession& callee, Key key,
                       MetaTopics& metaTopics)
    {
        auto found = byKey_.find(key);
        if (found == byKey_.end())
            return makeUnexpectedError(WampErrc::noSuchRegistration);

        auto& registration = found->second;
        if (registration.calleeId() != callee.wampId())
            return makeUnexpectedError(WampErrc::noSuchRegistration);

        Uri uri{registration.procedureUri()};

        if (metaTopics.enabled())
        {
            registration.resetCallee();
            metaTopics.onUnregister(callee.sharedInfo(),
                                    registration.info(false));
        }

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

    void removeCallee(const SessionInfo& calleeInfo, MetaTopics& metaTopics)
    {
        auto sid = calleeInfo.sessionId();
        auto iter1 = byUri_.begin();
        auto end1 = byUri_.end();
        while (iter1 != end1)
        {
            auto* reg = iter1->second;
            if (reg->calleeId() == sid)
                iter1 = byUri_.erase(iter1);
            else
                ++iter1;
        }

        auto iter2 = byKey_.begin();
        auto end2 = byKey_.end();
        while (iter2 != end2)
        {
            auto& reg = iter2->second;
            if (reg.calleeId() == sid)
            {
                if (metaTopics.enabled())
                {
                    reg.resetCallee();
                    metaTopics.onUnregister(calleeInfo, reg.info(false));
                }
                iter2 = byKey_.erase(iter2);
            }
            else
            {
                ++iter2;
            }
        }

        assert(byUri_.size() == byKey_.size());
    }

    ErrorOr<RegistrationInfo> at(RegistrationId rid, bool listCallees) const
    {
        auto found = byKey_.find(rid);
        if (found == byKey_.end())
            return makeUnexpectedError(WampErrc::noSuchRegistration);
        return found->second.info(listCallees);
    }

    ErrorOr<RegistrationInfo> lookup(const Uri& uri, bool listCallees) const
    {
        auto found = byUri_.find(uri);
        if (found == byUri_.end())
            return makeUnexpectedError(WampErrc::noSuchRegistration);
        return found->second->info(listCallees);
    }

    template <typename F>
    std::size_t forEachRegistration(F&& functor) const
    {
        std::size_t count = 0;
        for (const auto& kv: byKey_)
        {
            if (!(std::forward<F>(functor)(kv.second.info())))
                break;
            ++count;
        }
        return count;
    }

private:
    std::map<Key, DealerRegistration> byKey_;
    std::map<Uri, DealerRegistration*> byUri_; // TODO: Use trie
};

//------------------------------------------------------------------------------
using DealerJobKey = std::pair<SessionId, RequestId>;

//------------------------------------------------------------------------------
class DealerJob
{
public:
    using Timeout = std::chrono::steady_clock::duration;

    static ErrorOr<DealerJob> create(
        const RouterSession::Ptr& caller, const RouterSession::Ptr& callee,
        const Rpc& rpc, const DealerRegistration& reg)
    {
        WampErrc errc = WampErrc::success;
        DealerJob job{caller, callee, rpc, reg, errc};
        if (errc != WampErrc::success)
            return makeUnexpectedError(errc);
        return job;
    }

    DealerJob() = default;

    Invocation makeInvocation(const RouterSession& caller, Rpc&& rpc) const
    {
        // TODO: Propagate x_foo custom options?
        // https://github.com/wamp-proto/wamp-proto/issues/345

        Object customOptions;
        const auto trustLevel = rpc.trustLevel({});
        const bool callerDisclosed = rpc.discloseMe();
        const bool hasTrustLevel = rpc.hasTrustLevel({});

        auto found = rpc.options().find("custom");
        if (found != rpc.options().end() && found->second.is<Object>())
            customOptions = std::move(found->second.as<Object>());

        Invocation inv{{}, std::move(rpc), registrationId_};

        if (callerDisclosed)
        {
            // Disclosed properties are not in the spec, but there is
            // a consensus here:
            // https://github.com/wamp-proto/wamp-proto/issues/57
            const auto& info = caller.info();
            inv.withOption("caller", info.sessionId());
            if (!info.auth().id().empty())
                inv.withOption("caller_authid", info.auth().id());
            if (!info.auth().role().empty())
                inv.withOption("caller_authrole", info.auth().role());
        }

        if (isProgressiveCall_)
            inv.withOption("progress", true);

        if (progressiveResultsRequested_)
            inv.withOption("receive_progress", true);

        if (hasTrustLevel)
            inv.withOption("trust_level", trustLevel);

        if (!customOptions.empty())
            inv.withOption("custom", std::move(customOptions));

        return inv;
    }

    Invocation makeProgressiveInvocation(Rpc&& rpc)
    {
        // TODO: Repeat caller ID information?
        // https://github.com/wamp-proto/wamp-proto/issues/479

        assert(isProgressiveCall_);
        isProgressiveCall_ = rpc.isProgress({});
        Invocation inv{{}, std::move(rpc), registrationId_};
        inv.setRequestId({}, calleeKey_.second);

        // Only propagate the `progress` option. The initial progressive
        // call is what establishes other options for the duration of the
        // progressive call transfer.
        if (isProgressiveCall_)
            inv.withOption("progress", true);

        return inv;
    }

    void setRequestId(RequestId reqId)
    {
        calleeKey_.second = reqId;
    }

    ErrorOrDone cancel(CallCancelMode mode, WampErrc reason, bool& eraseNow)
    {
        using Mode = CallCancelMode;
        assert(mode != Mode::unknown);

        auto callee = this->callee_.lock();
        if (!callee)
            return false; // notifyAbandonedCallee has already sent ERROR

        auto calleeFeatures = callee->info().features().callee();
        const bool calleeHasCallCanceling =
            calleeFeatures.all_of(CalleeFeatures::callCanceling);
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
        auto calleeFeatures = callee->info().features().callee();
        if (calleeFeatures.all_of(CalleeFeatures::callCanceling))
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
        auto ec = make_error_code(WampErrc::cancelled);
        auto e = Error{PassKey{}, MessageKind::call, reqId, ec}
                     .withArgs("Callee left realm");
        caller->sendRouterCommand(std::move(e), true);
    }

    // Returns true if the job must be erased
    bool yield(Result&& result)
    {
        auto caller = this->caller_.lock();
        if (!caller || discardResultOrError_)
            return true;
        result.setKindToResult({});
        result.setRequestId({}, callerKey_.second);
        const bool isProgress = result.optionOr<bool>("progress", false);
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
        error.setRequestKindToCall({});
        caller->sendRouterCommand(std::move(error), true);
    }

    RouterSession::WeakPtr caller() const {return caller_;}

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
    DealerJob(const RouterSession::Ptr& caller,
              const RouterSession::Ptr& callee, const Rpc& rpc,
              const DealerRegistration& reg, WampErrc& errc)
        : caller_(caller),
          callee_(callee),
          callerKey_(caller->wampId(), rpc.requestId({})),
          calleeKey_(callee->wampId(), nullId()),
          registrationId_(reg.info().id)
    {
        auto timeout = rpc.dealerTimeout();
        hasTimeout_ = timeout.has_value() && (*timeout != Timeout{0});
        if (hasTimeout_)
            timeout_ = *timeout;

        auto calleeFeatures = callee->info().features().callee();
        const bool calleeHasCallCancelling =
            calleeFeatures.all_of(CalleeFeatures::callCanceling);

        // Not clear what the behavior should be when progressive results are
        // requested, but not supported by the callee.
        // https://github.com/wamp-proto/wamp-proto/issues/467
        if (rpc.progressiveResultsAreEnabled({}))
        {
            const bool calleeHasProgressiveCallResults =
                calleeHasCallCancelling &&
                calleeFeatures.all_of(CalleeFeatures::progressiveCallResults);
            progressiveResultsRequested_ = calleeHasProgressiveCallResults;
        }

        if (rpc.isProgress({}))
        {
            const bool calleeHasProgressiveCallInvocations =
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

    explicit DealerJobMap(IoStrand strand)
        : timeoutScheduler_(TimeoutScheduler<Key>::create(std::move(strand)))
    {
        timeoutScheduler_->listen([this](Key k){onTimeout(k);});
    }

    void insert(Job&& job, RequestId reqId)
    {
        job.setRequestId(reqId);

        if (job.hasTimeout())
            timeoutScheduler_->insert(job.calleeKey(), job.timeout());

        auto callerKey = job.callerKey();
        auto calleeKey = job.calleeKey();
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
            const SessionId calleeSessionId = iter->first.first;
            const SessionId callerSessionId = iter->second->first.first;
            const bool calleeMatches = calleeSessionId == sessionId;
            const bool callerMatches = callerSessionId == sessionId;

            if (calleeMatches || callerMatches)
            {
                auto& job = iter->second->second;

                if (!callerMatches && calleeMatches)
                    job.notifyAbandonedCallee();

                if (callerMatches && !calleeMatches)
                    job.notifyAbandonedCaller();

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

private:
    void onTimeout(Key calleeKey)
    {
        auto iter = byCallee_.find(calleeKey);
        if (iter != byCallee_.end())
        {
            auto& job = iter->second->second;
            bool eraseNow = false;
            auto done = job.cancel(CallCancelMode::killNoWait,
                                   WampErrc::timeout, eraseNow);
            auto caller = job.caller().lock();
            if (eraseNow)
                byCalleeErase(iter);
            if (caller && !done)
            {
                Error e{PassKey{}, MessageKind::call, job.callerKey().second,
                        done.error()};
                caller->sendRouterCommand(std::move(e), true);
            }
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
    Dealer(IoStrand strand, MetaTopics::Ptr metaTopics)
        : jobs_(std::move(strand)),
          metaTopics_(std::move(metaTopics))
    {}

    ErrorOr<RegistrationId> enroll(const RouterSession::Ptr& callee,
                                   Procedure&& p)
    {
        if (registry_.contains(p.uri()))
            return makeUnexpectedError(WampErrc::procedureAlreadyExists);
        DealerRegistration reg{std::move(p), callee};
        auto key = nextRegistrationId();
        const auto& inserted = registry_.insert(key, std::move(reg));
        if (metaTopics_->enabled())
            metaTopics_->onRegister(callee->sharedInfo(), inserted.info(false));
        return key;
    }

    ErrorOr<Uri> unregister(const RouterSession::Ptr& callee,
                            RegistrationId rid)
    {
        // Consensus on what to do with pending invocations upon unregister
        // appears to be to allow them to continue.
        // https://github.com/wamp-proto/wamp-proto/issues/283#issuecomment-429542748
        return registry_.erase(*callee, rid, *metaTopics_);
    }

    ErrorOrDone call(const RouterSession::Ptr& caller, Rpc& rpc)
    {
        auto* reg = registry_.find(rpc.uri());
        if (reg == nullptr)
            return makeUnexpectedError(WampErrc::noSuchProcedure);

        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(WampErrc::noSuchProcedure);

        auto rpcReqId = rpc.requestId({});

        const bool isContinuation = rpcReqId <=
                                    caller->lastInsertedCallRequestId();
        if (isContinuation)
            return continueCall(*caller, *callee, std::move(rpc));

        return newCall(caller, callee, std::move(rpc), *reg);
    }

    ErrorOrDone cancelCall(const RouterSession::Ptr& caller,
                           CallCancellation&& cncl)
    {
        const DealerJobKey callerKey{caller->wampId(), cncl.requestId({})};
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

    void yieldResult(const RouterSession::Ptr& callee, Result&& result)
    {
        const DealerJobKey calleeKey{callee->wampId(), result.requestId({})};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        if (job.yield(std::move(result)))
            jobs_.byCalleeErase(iter);
        else
            jobs_.updateProgressiveResultDeadline(job);
    }

    void yieldError(const RouterSession::Ptr& callee, Error&& error)
    {
        const DealerJobKey calleeKey{callee->wampId(), error.requestId({})};
        auto iter = jobs_.byCalleeFind(calleeKey);
        if (iter == jobs_.byCalleeEnd())
            return;
        auto& job = iter->second->second;
        job.yield(std::move(error));
        jobs_.byCalleeErase(iter);
    }

    void removeSession(const SessionInfo& info)
    {
        registry_.removeCallee(info, *metaTopics_);
        jobs_.removeSession(info.sessionId());
    }

    ErrorOr<RegistrationInfo> getRegistration(RegistrationId rid,
                                              bool listCallees) const
    {
        return registry_.at(rid, listCallees);
    }

    ErrorOr<RegistrationInfo> lookupRegistration(
        const Uri& uri, MatchPolicy p, bool listCallees) const
    {
        if (p != MatchPolicy::exact)
            return makeUnexpectedError(WampErrc::noSuchRegistration);
        return registry_.lookup(uri, listCallees);
    }

    ErrorOr<RegistrationInfo> bestRegistrationMatch(const Uri& uri,
                                                    bool listCallees) const
    {
        return registry_.lookup(uri, listCallees);
    }

    template <typename F>
    std::size_t forEachRegistration(MatchPolicy p, F&& functor) const
    {
        if (p != MatchPolicy::exact)
            return 0;
        return registry_.forEachRegistration(std::forward<F>(functor));
    }

private:
    using MutexGuard = std::lock_guard<std::mutex>;

    RegistrationId nextRegistrationId() {return ++nextRegistrationId_;}

    ErrorOrDone newCall(const RouterSession::Ptr& caller,
                        const RouterSession::Ptr& callee,
                        Rpc&& rpc, const DealerRegistration& reg)
    {
        auto uri = rpc.uri();
        auto job = DealerJob::create(caller, callee, rpc, reg);
        if (!job)
            return makeUnexpected(job.error());
        caller->setLastInsertedCallRequestId(rpc.requestId({}));
        auto inv = job->makeInvocation(*caller, std::move(rpc));
        auto reqId = callee->sendInvocation(std::move(inv), std::move(uri));
        jobs_.insert(std::move(*job), reqId);
        return true;
    }

    ErrorOrDone continueCall(RouterSession& caller, RouterSession& callee,
                             Rpc&& rpc)
    {
        auto uri = rpc.uri();
        auto found = jobs_.byCallerFind({caller.wampId(), rpc.requestId({})});

        /*  Ignore requests for call continuations when the call has already
            ended. Due to races, the caller may not be aware that the call is
            ended when it sent the CALL, but the caller will eventually become
            aware of the call having ended and can react accordingly.
            https://github.com/wamp-proto/wamp-proto/issues/482 */
        if (found == jobs_.byCallerEnd())
            return false;

        auto& job = found->second;
        if (!job.isProgressiveCall())
        {
            auto unex = makeUnexpectedError(WampErrc::protocolViolation);
            caller.abort(Reason{unex.value()}.withHint(
                "Cannot reinvoke an RPC that is closed to further progress"));
            return unex;
        }
        auto inv = job.makeProgressiveInvocation(std::move(rpc));
        callee.sendRouterCommand(std::move(inv), std::move(uri));
        return true;
    }

    mutable std::mutex queryMutex_;
    DealerRegistry registry_;
    DealerJobMap jobs_;
    RegistrationId nextRegistrationId_ = nullId();
    MetaTopics::Ptr metaTopics_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DEALER_HPP
