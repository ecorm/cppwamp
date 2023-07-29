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
#include <memory>
#include <mutex>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../errorinfo.hpp"
#include "../erroror.hpp"
#include "../routerconfig.hpp"
#include "../rpcinfo.hpp"
#include "../uri.hpp"
#include "../utils/triemap.hpp"
#include "authorizationlistener.hpp"
#include "commandinfo.hpp"
#include "disclosuremode.hpp"
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
        : info_(0, procedure, std::chrono::system_clock::now()),
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

    RouterSession::WeakPtr callee() const {return callee_;}

    SessionId calleeId() const {return calleeId_;}

    const RegistrationInfo& info() const {return info_;}

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

    DealerRegistration* insert(Key key, DealerRegistration&& reg)
    {
        reg.setRegistrationId(key);
        auto emplaced = byKey_.emplace(key, std::move(reg));
        assert(emplaced.second);
        DealerRegistration* ptr = &(emplaced.first->second);
        byUri_.emplace(ptr->info().uri, ptr);
        assert(byUri_.size() == byKey_.size());
        return &(emplaced.first->second);
    }

    ErrorOr<Uri> erase(RouterSession& callee, Key key,
                       MetaTopics& metaTopics,
                       const Authorizer::Ptr& authorizer)
    {
        auto found = byKey_.find(key);
        if (found == byKey_.end())
            return makeUnexpectedError(WampErrc::noSuchRegistration);

        auto& registration = found->second;
        if (registration.calleeId() != callee.wampId())
            return makeUnexpectedError(WampErrc::noSuchRegistration);

        Uri uri{registration.procedureUri()};

        if (authorizer)
            authorizer->uncacheProcedure(registration.info());

        if (metaTopics.enabled())
        {
            registration.resetCallee();
            metaTopics.onUnregister(callee.sharedInfo(),
                                    registration.info().withoutCallees());
        }

        auto erased = byUri_.erase(uri);
        assert(erased == 1);
        byKey_.erase(found);
        assert(byUri_.size() == byKey_.size());

        return uri;
    }

    DealerRegistration* find(const Uri& procedure) const
    {
        auto found = byUri_.find(procedure);
        if (found == byUri_.end())
            return nullptr;
        return found.value();
    }

    void removeCallee(const SessionInfo& calleeInfo, MetaTopics& metaTopics)
    {
        auto sid = calleeInfo.sessionId();
        auto iter1 = byUri_.begin();
        auto end1 = byUri_.end();
        while (iter1 != end1)
        {
            auto* reg = iter1.value();
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
                    metaTopics.onUnregister(calleeInfo,
                                            reg.info().withoutCallees());
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
        const auto& info = found->second.info();
        if (listCallees)
            return info;
        return info.withoutCallees();
    }

    ErrorOr<RegistrationInfo> lookup(const Uri& uri, bool listCallees) const
    {
        auto found = byUri_.find(uri);
        if (found == byUri_.end())
            return makeUnexpectedError(WampErrc::noSuchRegistration);
        const auto& info = found.value()->info();
        if (listCallees)
            return info;
        return info.withoutCallees();
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
    utils::TrieMap<DealerRegistration*> byUri_;
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
        const Rpc& rpc, const DealerRegistration& reg,
        bool calleeTimeoutArmed)
    {
        WampErrc errc = WampErrc::success;
        DealerJob job{caller, callee, rpc, reg, errc, calleeTimeoutArmed};
        if (errc != WampErrc::success)
            return makeUnexpectedError(errc);
        return job;
    }

    DealerJob() = default;

    Invocation makeInvocation(const RouterSession& caller, Rpc&& rpc,
                              bool calleeTimeoutArmed) const
    {
        // TODO: WAMP - Propagate x_foo custom options?
        // https://github.com/wamp-proto/wamp-proto/issues/345

        const auto trustLevel = rpc.trustLevel({});
        const bool callerDisclosed = rpc.discloseMe();
        const bool hasTrustLevel = rpc.hasTrustLevel({});
        ErrorOr<Object> customOptions = rpc.optionAs<Object>("custom");

        Variant timeout;
        if (calleeTimeoutArmed)
            timeout = rpc.optionByKey("timeout");

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

        if (customOptions.has_value())
            inv.withOption("custom", *std::move(customOptions));

        if (!timeout.is<Null>())
            inv.withOption("timeout", std::move(timeout));

        return inv;
    }

    Invocation makeProgressiveInvocation(Rpc&& rpc)
    {
        // TODO: WAMP - Repeat caller ID information?
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

        const bool calleeHasCallCanceling =
            callee->info().features().callee().test(Feature::callCanceling);
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
        if (callee->info().features().callee().test(Feature::callCanceling))
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
              const DealerRegistration& reg, WampErrc& errc,
              bool calleeTimeoutArmed)
        : caller_(caller),
          callee_(callee),
          callerKey_(caller->wampId(), rpc.requestId({})),
          calleeKey_(callee->wampId(), nullId()),
          registrationId_(reg.info().id)
    {
        if (!calleeTimeoutArmed)
        {
            auto timeout = rpc.dealerTimeout();
            hasTimeout_ = timeout.has_value() && (*timeout != Timeout{0});
            if (hasTimeout_)
                timeout_ = *timeout;
        }

        const auto calleeFeatures = callee->info().features().callee();
        const bool calleeHasCallCancelling =
            calleeFeatures.test(Feature::callCanceling);

        // Not clear what the behavior should be when progressive results are
        // requested, but not supported by the callee.
        // https://github.com/wamp-proto/wamp-proto/issues/467
        if (rpc.progressiveResultsAreEnabled({}))
        {
            const bool calleeHasProgressiveCallResults =
                calleeHasCallCancelling &&
                calleeFeatures.test(Feature::progressiveCallResults);
            progressiveResultsRequested_ = calleeHasProgressiveCallResults;
        }

        if (rpc.isProgress({}))
        {
            const bool calleeHasProgressiveCallInvocations =
                calleeHasCallCancelling &&
                calleeFeatures.test(Feature::progressiveCallInvocations);

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
            const RequestId reqId = job.callerKey().second;
            bool eraseNow = false;
            auto done = job.cancel(CallCancelMode::killNoWait,
                                   WampErrc::timeout, eraseNow);
            auto caller = job.caller().lock();
            if (eraseNow)
                byCalleeErase(iter);
            if (caller && !done)
            {
                Error e{PassKey{}, MessageKind::call, reqId, done.error()};
                caller->sendRouterCommand(std::move(e), true);
            }
        }
    }

    ByCallee byCallee_;
    ByCaller byCaller_;
    TimeoutScheduler<Key>::Ptr timeoutScheduler_;
};

//------------------------------------------------------------------------------
class DealerImpl
{
public:
    DealerImpl(IoStrand strand, MetaProcedures::Ptr metaProcedures,
               MetaTopics::Ptr metaTopics, const RealmConfig& cfg)
        : jobs_(std::move(strand)),
          metaProcedures_(std::move(metaProcedures)),
          metaTopics_(std::move(metaTopics)),
          authorizer_(cfg.authorizer()),
          callTimeoutForwardingRule_(cfg.callTimeoutForwardingRule())
    {}

    bool metaProceduresAreEnabled() const
    {
        return metaProcedures_ != nullptr;
    }

    bool hasMetaProcedure(const Uri& uri) const
    {
        return metaProcedures_ && metaProcedures_->hasProcedure(uri);
    }

    DealerRegistration* findProcedure(const Uri& uri) const
    {
        return registry_.find(uri);
    }

    const Authorizer::Ptr& authorizer() const {return authorizer_;}

    void enroll(const RouterSession::Ptr& callee, Procedure&& proc)
    {
        if (registry_.contains(proc.uri()))
        {
            callee->sendRouterCommandError(proc,
                                           WampErrc::procedureAlreadyExists);
            return;
        }

        auto reqId = proc.requestId({});
        DealerRegistration reg{std::move(proc), callee};
        auto regId = nextRegistrationId();
        DealerRegistration* inserted = nullptr;

        {
            const MutexGuard guard{queryMutex_};
            inserted = registry_.insert(regId, std::move(reg));
        }

        callee->sendRouterCommand(Registered{reqId, regId},
                                  inserted->info().uri);
        if (metaTopics_->enabled())
        {
            metaTopics_->onRegister(callee->sharedInfo(),
                                    inserted->info().withoutCallees());
        }
    }

    void unregister(const RouterSession::Ptr& callee, const Unregister& cmd)
    {
        // Consensus on what to do with pending invocations upon unregister
        // appears to be to allow them to continue.
        // https://github.com/wamp-proto/wamp-proto/issues/283#issuecomment-429542748

        ErrorOr<Uri> uri;

        {
            const MutexGuard guard{queryMutex_};
            uri = registry_.erase(*callee, cmd.registrationId(), *metaTopics_,
                                  authorizer_);
        }

        if (uri.has_value())
            callee->sendRouterCommand(Unregistered{cmd.requestId({})}, *uri);
        else
            callee->sendRouterCommandError(cmd, uri.error());
    }

    void call(const RouterSession::Ptr& caller, Rpc&& rpc,
              DealerRegistration* reg = nullptr)
    {
        const auto reqId = rpc.requestId({});
        const auto done = callProcedure(caller, std::move(rpc), reg);
        if (!done.has_value())
        {
            caller->sendRouterCommand(
                Error{PassKey{}, MessageKind::call, reqId, done.error()}, true);
        }
    }

    void cancelCall(const RouterSession::Ptr& caller, CallCancellation&& cncl)
    {
        const DealerJobKey callerKey{caller->wampId(), cncl.requestId({})};
        auto iter = jobs_.byCallerFind(callerKey);
        if (iter == jobs_.byCallerEnd())
            return;

        using CM = CallCancelMode;
        auto mode = (cncl.mode() == CM::unknown) ? CM::killNoWait : cncl.mode();
        auto& job = iter->second;
        bool eraseNow = false;
        auto done = job.cancel(mode, WampErrc::cancelled, eraseNow);
        if (eraseNow)
            jobs_.byCallerErase(iter);
        if (!done.has_value())
        {
            caller->sendRouterCommand(
                Error{PassKey{}, MessageKind::call, cncl.requestId({}),
                      done.error()}, true);
        }
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
        {
            const MutexGuard guard{queryMutex_};
            registry_.removeCallee(info, *metaTopics_);
        }

        jobs_.removeSession(info.sessionId());
    }

    ErrorOr<RegistrationInfo> getRegistration(RegistrationId rid,
                                              bool listCallees) const
    {
        const MutexGuard guard{queryMutex_};
        return registry_.at(rid, listCallees);
    }

    ErrorOr<RegistrationInfo> lookupRegistration(
        const Uri& uri, MatchPolicy p, bool listCallees) const
    {
        const MutexGuard guard{queryMutex_};
        if (p != MatchPolicy::exact)
            return makeUnexpectedError(WampErrc::noSuchRegistration);
        return registry_.lookup(uri, listCallees);
    }

    ErrorOr<RegistrationInfo> bestRegistrationMatch(const Uri& uri,
                                                    bool listCallees) const
    {
        const MutexGuard guard{queryMutex_};
        return registry_.lookup(uri, listCallees);
    }

    template <typename F>
    std::size_t forEachRegistration(MatchPolicy p, F&& functor) const
    {
        const MutexGuard guard{queryMutex_};
        if (p != MatchPolicy::exact)
            return 0;
        return registry_.forEachRegistration(std::forward<F>(functor));
    }

private:
    using MutexGuard = std::lock_guard<std::mutex>;

    ErrorOrDone callProcedure(const RouterSession::Ptr& caller, Rpc&& rpc,
                              DealerRegistration* reg)
    {
        if (reg == nullptr)
            reg = registry_.find(rpc.uri());

        if (reg == nullptr)
        {
            auto found = metaProcedures_ &&
                         metaProcedures_->call(*caller, std::move(rpc));
            if (!found)
                return makeUnexpectedError(WampErrc::noSuchProcedure);
            return true;
        }

        auto callee = reg->callee().lock();
        if (!callee)
            return makeUnexpectedError(WampErrc::noSuchProcedure);

        auto rpcReqId = rpc.requestId({});

        const bool isContinuation = rpcReqId <=
                                    caller->lastInsertedCallRequestId();
        if (isContinuation)
            return continueCall(*caller, *callee, std::move(rpc));

        return newCall(caller, callee, std::move(rpc), *reg);
    };

    ErrorOrDone newCall(const RouterSession::Ptr& caller,
                        const RouterSession::Ptr& callee,
                        Rpc&& rpc, const DealerRegistration& reg)
    {
        const bool calleeTimeoutArmed = computeCalleeTimeoutArmed(*callee, reg);

        auto uri = rpc.uri();
        auto job = DealerJob::create(caller, callee, rpc, reg,
                                     calleeTimeoutArmed);
        if (!job)
            return makeUnexpected(job.error());
        caller->setLastInsertedCallRequestId(rpc.requestId({}));
        auto inv = job->makeInvocation(*caller, std::move(rpc),
                                       calleeTimeoutArmed);
        auto reqId = callee->sendInvocation(std::move(inv), std::move(uri));
        jobs_.insert(std::move(*job), reqId);
        return true;
    }

    bool computeCalleeTimeoutArmed(const RouterSession& callee,
                                   const DealerRegistration& reg)
    {
        using Rule = CallTimeoutForwardingRule;

        switch (callTimeoutForwardingRule_)
        {
        case Rule::perRegistration:
            return reg.info().forwardTimeoutEnabled;

        case Rule::perFeature:
            return callee.info().features().callee().test(Feature::callTimeout);

        case Rule::never:
            return false;

        default:
            assert(false && "Unexpected CallTimeoutForwardingRule enumerator");
            break;
        }

        return false;
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
            caller.abort(Reason{WampErrc::protocolViolation}.withHint(
                "Cannot reinvoke an RPC that is closed to further progress"));
            return false;
        }
        auto inv = job.makeProgressiveInvocation(std::move(rpc));
        callee.sendRouterCommand(std::move(inv), std::move(uri));
        return true;
    }

    RegistrationId nextRegistrationId() {return ++nextRegistrationId_;}

    mutable std::mutex queryMutex_;
    DealerRegistry registry_;
    DealerJobMap jobs_;
    RegistrationId nextRegistrationId_ = nullId();
    MetaProcedures::Ptr metaProcedures_;
    MetaTopics::Ptr metaTopics_;
    Authorizer::Ptr authorizer_;
    CallTimeoutForwardingRule callTimeoutForwardingRule_ = {};
};

//------------------------------------------------------------------------------
class Dealer : public std::enable_shared_from_this<Dealer>,
               public AuthorizationListener
{
public:
    using Ptr = std::shared_ptr<Dealer>;
    using SharedStrand = std::shared_ptr<IoStrand>;

    Dealer(AnyIoExecutor executor, SharedStrand strand,
           MetaProcedures::Ptr metaProcedures, MetaTopics::Ptr metaTopics,
           UriValidator::Ptr uriValidator, const RealmConfig& cfg)
        : impl_(*strand, std::move(metaProcedures), std::move(metaTopics), cfg),
          executor_(std::move(executor)),
          strand_(std::move(strand)),
          uriValidator_(std::move(uriValidator)),
          callerDisclosure_(cfg.callerDisclosure()),
          metaProcedureRegistrationAllowed_(
              cfg.metaProcedureRegistrationAllowed())
    {}

    void enroll(RouterSession::Ptr callee, Procedure&& procedure)
    {
        dispatchCommand(std::move(callee), std::move(procedure));
    }

    void unregister(RouterSession::Ptr callee, Unregister&& cmd)
    {
        dispatchCommand(std::move(callee), std::move(cmd));
    }

    void call(RouterSession::Ptr caller, Rpc&& call)
    {
        dispatchCommand(std::move(caller), std::move(call));
    }

    void cancelCall(RouterSession::Ptr caller, CallCancellation&& cancel)
    {
        dispatchCommand(std::move(caller), std::move(cancel));
    }

    void yieldResult(RouterSession::Ptr callee, Result&& result)
    {
        dispatchCommand(std::move(callee), std::move(result));
    }

    void yieldError(RouterSession::Ptr callee, Error&& error)
    {
        dispatchCommand(std::move(callee), std::move(error));
    }

    void removeSession(const SessionInfo& info) {impl_.removeSession(info);}

    ErrorOr<RegistrationInfo> getRegistration(RegistrationId rid,
                                              bool listCallees) const
    {
        return impl_.getRegistration(rid, listCallees);
    }

    ErrorOr<RegistrationInfo> lookupRegistration(
        const Uri& uri, MatchPolicy p, bool listCallees) const
    {
        return impl_.lookupRegistration(uri, p, listCallees);
    }

    ErrorOr<RegistrationInfo> bestRegistrationMatch(const Uri& uri,
                                                    bool listCallees) const
    {
        return impl_.bestRegistrationMatch(uri, listCallees);
    }

    template <typename F>
    std::size_t forEachRegistration(MatchPolicy p, F&& functor) const
    {
        return impl_.forEachRegistration(p, std::forward<F>(functor));
    }

private:
    template <typename C>
    void dispatchCommand(RouterSession::Ptr originator, C&& command)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr o;
            ValueTypeOf<C> c;
            void operator()() {self->processCommand(o, std::move(c));}
        };

        boost::asio::dispatch(
            *strand_,
            Dispatched{shared_from_this(), std::move(originator),
                       std::forward<C>(command)});
    }

    void processCommand(const RouterSession::Ptr& callee, Procedure&& enroll)
    {
        if (enroll.matchPolicy() != MatchPolicy::exact)
        {
            callee->sendRouterCommandError(
                enroll, WampErrc::optionNotAllowed,
                "pattern-based registrations are not supported");
            return;
        }

        if (!uriValidator_->checkProcedure(enroll.uri(), false))
            return callee->abort({WampErrc::invalidUri});

        auto errc = checkMetaProcedureRegistrationAttempt(enroll);
        if (errc != WampErrc::success)
            return callee->sendRouterCommandError(enroll, errc);

        authorize(callee, enroll, false);
    }

    void processCommand(const RouterSession::Ptr& callee, Unregister&& cmd)
    {
        impl_.unregister(callee, cmd);
    }

    void processCommand(const RouterSession::Ptr& caller, Rpc&& call)
    {
        if (!uriValidator_->checkProcedure(call.uri(), false))
            return caller->abort({WampErrc::invalidUri});

        auto reg = impl_.findProcedure(call.uri());
        if (!reg && !impl_.hasMetaProcedure(call.uri()))
        {
            caller->sendRouterCommandError(call, WampErrc::noSuchProcedure);
            return;
        }

        const bool discloseCaller = (reg != nullptr) &&
                                    reg->info().discloseCaller;
        authorize(caller, call, discloseCaller, reg);
    }

    void processCommand(const RouterSession::Ptr& caller,
                        CallCancellation&& cmd)
    {
        impl_.cancelCall(caller, std::move(cmd));
    }

    void processCommand(const RouterSession::Ptr& callee, Result&& yielded)
    {
        return impl_.yieldResult(callee, std::move(yielded));
    }

    void processCommand(const RouterSession::Ptr& callee, Error&& yielded)
    {
        if (!uriValidator_->checkError(yielded.uri()))
            return callee->abort({WampErrc::invalidUri});
        return impl_.yieldError(callee, std::move(yielded));
    }

    template <typename C, typename... Ts>
    void authorize(const RouterSession::Ptr& originator, C& command,
                   bool consumerDisclosure, Ts&&... bypassArgs)
    {
        const auto& authorizer = impl_.authorizer();
        if (!authorizer)
        {
            return bypassAuthorization(originator, std::move(command),
                                       consumerDisclosure,
                                       std::forward<Ts>(bypassArgs)...);
        }

        AuthorizationRequest r{{}, shared_from_this(), originator,
                               authorizer, callerDisclosure_.disclosure(),
                               consumerDisclosure};
        authorizer->authorize(std::forward<C>(command), std::move(r));
    }

    void bypassAuthorization(const RouterSession::Ptr& callee, Procedure&& p,
                             bool /* consumerDisclosure */)
    {
        impl_.enroll(callee, std::move(p));
    }

    void bypassAuthorization(const RouterSession::Ptr& caller, Rpc&& rpc,
                             bool consumerDisclosure, DealerRegistration* reg)
    {
        bool disclosed = callerDisclosure_.compute(rpc.disclosed({}),
                                                   consumerDisclosure);
        rpc.setDisclosed({}, disclosed);
        impl_.call(caller, std::move(rpc), reg);
    }

    WampErrc checkMetaProcedureRegistrationAttempt(const Procedure& enroll)
    {
        if (metaProcedureRegistrationAllowed_)
        {
            if (!impl_.metaProceduresAreEnabled())
                return WampErrc::success;
            if (impl_.hasMetaProcedure(enroll.uri()))
                return WampErrc::procedureAlreadyExists;
        }
        else if (enroll.isMeta())
        {
            return WampErrc::invalidUri;
        }

        return WampErrc::success;
    }

    void onAuthorized(const RouterSession::Ptr& callee,
                      Procedure&& proc) override
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr callee;
            Procedure proc;
            void operator()() {self->impl_.enroll(callee, std::move(proc));}
        };

        boost::asio::dispatch(
            *strand_, Dispatched{shared_from_this(), callee, std::move(proc)});
    };

    void onAuthorized(const RouterSession::Ptr& caller, Rpc&& rpc) override
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr caller;
            Rpc rpc;

            void operator()()
            {
                self->impl_.call(caller, std::move(rpc));
            }
        };

        boost::asio::dispatch(
            *strand_, Dispatched{shared_from_this(), caller, std::move(rpc)});
    };

    DealerImpl impl_;
    AnyIoExecutor executor_;
    SharedStrand strand_;
    UriValidator::Ptr uriValidator_;
    DisclosureMode callerDisclosure_;
    bool metaProcedureRegistrationAllowed_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DEALER_HPP
