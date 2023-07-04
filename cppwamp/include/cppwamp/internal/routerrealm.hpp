/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "../anyhandler.hpp"
#include "../authorizer.hpp"
#include "../realmobserver.hpp"
#include "../routerconfig.hpp"
#include "broker.hpp"
#include "commandinfo.hpp"
#include "metaapi.hpp"
#include "dealer.hpp"
#include "random.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RouterRealm : public std::enable_shared_from_this<RouterRealm>,
                    public MetaPublisher
{
public:
    using Ptr              = std::shared_ptr<RouterRealm>;
    using WeakPtr          = std::weak_ptr<RouterRealm>;
    using Executor         = AnyIoExecutor;
    using FallbackExecutor = AnyCompletionExecutor;
    using ObserverId       = MetaTopics::ObserverId;
    using SessionPredicate = std::function<bool (const SessionInfo&)>;
    using SessionIdSet     = std::set<SessionId>;

    static Ptr create(Executor e, RealmConfig c, const RouterConfig& rcfg,
                      RouterContext rctx, RandomNumberGenerator64 rng)
    {
        return Ptr(new RouterRealm(std::move(e), std::move(c), rcfg,
                                   std::move(rctx), std::move(rng)));
    }

    ~RouterRealm() override = default;

    const Executor& executor() const {return executor_;}

    const IoStrand& strand() const {return strand_;}

    const std::string& uri() const {return config_.uri();}

    bool isOpen() const {return isOpen_.load();}

    void join(RouterSession::Ptr session)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr session;
            void operator()() {self->joinSession(session);}
        };

        safelyDispatch<Dispatched>(std::move(session));
    }

    void close(Reason r)
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            void operator()() {self->closeRealm(std::move(r));}
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    bool closeViaRouter(Reason r)
    {
        return router_.closeRealm(uri(), std::move(r));
    }

    void observe(RealmObserver::Ptr o, FallbackExecutor e)
    {
        struct Dispatched
        {
            Ptr self;
            RealmObserver::Ptr o;
            FallbackExecutor e;
            void operator()() {self->metaTopics_->addObserver(o, e);}
        };

        safelyDispatch<Dispatched>(std::move(o), std::move(e));
    }

    std::size_t sessionCount() const
    {
        const std::lock_guard<std::mutex> guard{sessionQueryMutex_};
        return sessions_.size();
    }

    template <typename F>
    std::size_t forEachSession(F&& functor) const
    {
        const std::lock_guard<std::mutex> guard{sessionQueryMutex_};
        std::size_t count = 0;
        for (const auto& kv: sessions_)
        {
            if (!std::forward<F>(functor)(kv.second->sharedInfo()))
                break;
            ++count;
        }
        return count;
    }

    SessionInfo getSession(SessionId sid) const
    {
        const MutexGuard guard{sessionQueryMutex_};
        auto found = sessions_.find(sid);
        return (found == sessions_.end()) ? SessionInfo{}
                                          : found->second->sharedInfo();
    }

    ErrorOr<bool> killSessionById(SessionId sid, Reason r)
    {
        struct Dispatched
        {
            Ptr self;
            SessionId sid;
            Reason r;
            void operator()() {self->doKillSession(sid, std::move(r));}
        };

        {
            const MutexGuard guard{sessionQueryMutex_};
            auto iter = sessions_.find(sid);
            if (iter == sessions_.end())
                return makeUnexpectedError(WampErrc::noSuchSession);
        }

        safelyDispatch<Dispatched>(sid, std::move(r));
        return true;
    }

    template <typename F>
    SessionIdSet killSessionIf(F&& filter, Reason r)
    {
        struct Dispatched
        {
            Ptr self;
            SessionIdSet set;;
            Reason r;
            void operator()() {self->doKillSessions(set, r);}
        };

        SessionIdSet set;

        {
            const MutexGuard guard{sessionQueryMutex_};
            for (const auto& kv: sessions_)
            {
                const auto& session = kv.second;
                if (std::forward<F>(filter)(session->sharedInfo()))
                    set.insert(session->wampId());
            }
        }

        if (!set.empty())
            safelyDispatch<Dispatched>(set, std::move(r));

        return set;
    }

    SessionIdSet killSessions(SessionIdSet set, Reason r)
    {
        struct Dispatched
        {
            Ptr self;
            SessionIdSet set;
            Reason r;
            void operator()() {self->doKillSessions(set, r);}
        };

        {
            const MutexGuard guard{sessionQueryMutex_};
            auto end = set.end();
            auto iter = set.begin();
            while (iter != end)
            {
                const SessionId sid = *iter;
                if (sessions_.count(sid) != 0)
                    ++iter;
                else
                    iter = set.erase(iter);
            }
        }

        if (!set.empty())
            safelyDispatch<Dispatched>(set, std::move(r));

        return set;
    }

    ErrorOr<RegistrationInfo> getRegistration(RegistrationId rid,
                                              bool listCallees = false)
    {
        return dealer_.getRegistration(rid, listCallees);
    }

    ErrorOr<RegistrationInfo> lookupRegistration(const Uri& uri, MatchPolicy p,
                                                 bool listCallees = false)
    {
        return dealer_.lookupRegistration(uri, p, listCallees);
    }

    ErrorOr<RegistrationInfo> bestRegistrationMatch(const Uri& uri,
                                                    bool listCallees = false)
    {
        return dealer_.bestRegistrationMatch(uri, listCallees);
    }

    template <typename F>
    std::size_t forEachRegistration(MatchPolicy p, F&& functor) const
    {
        return dealer_.forEachRegistration(p, std::forward<F>(functor));
    }

    ErrorOr<SubscriptionInfo> getSubscription(SubscriptionId sid,
                                              bool listSubscribers = false)
    {
        return broker_.getSubscription(sid, listSubscribers);
    }

    ErrorOr<SubscriptionInfo> lookupSubscription(const Uri& uri, MatchPolicy p,
                                                 bool listSubscribers = false)
    {
        return broker_.lookupSubscription(uri, p, listSubscribers);
    }

    template <typename F>
    std::size_t forEachSubscription(MatchPolicy p, F&& functor) const
    {
        return broker_.forEachSubscription(p, std::forward<F>(functor));
    }

    template <typename F>
    std::size_t forEachMatchingSubscription(const Uri& uri, F&& functor) const
    {
        return broker_.forEachMatch(uri, std::forward<F>(functor));
    }

    RouterRealm(const RouterRealm&) = delete;
    RouterRealm(RouterRealm&&) = delete;
    RouterRealm& operator=(const RouterRealm&) = delete;
    RouterRealm& operator=(RouterRealm&&) = delete;

private:
    using SessionMap = std::map<SessionId, RouterSession::Ptr>;
    using RealmProcedures = MetaProcedures<RouterRealm>;
    using MutexGuard = std::lock_guard<std::mutex>;
    using RealmProceduresPtr = std::unique_ptr<RealmProcedures>;

    RouterRealm(Executor&& e, RealmConfig&& c, const RouterConfig& rcfg,
                RouterContext&& rctx, RandomNumberGenerator64&& rng)
        : executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          config_(std::move(c)),
          router_(std::move(rctx)),
          metaTopics_(std::make_shared<MetaTopics>(this, executor_, strand_,
                                                   config_.metaApiEnabled())),
          broker_(std::move(rng), metaTopics_),
          dealer_(strand_, metaTopics_, config_),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          logger_(router_.logger()),
          uriValidator_(rcfg.uriValidator()),
          isOpen_(true)
    {
        if (config_.metaApiEnabled())
            metaProcedures_ = RealmProceduresPtr(new RealmProcedures(this));
    }

    RouterLogger::Ptr logger() const {return logger_;}

    void joinSession(const RouterSession::Ptr& session)
    {
        auto reservedId = router_.reserveSessionId();
        auto id = reservedId.get();
        session->setWampId(std::move(reservedId));

        {
            const MutexGuard guard{sessionQueryMutex_};
            sessions_.emplace(id, session);
        }

        if (metaTopics_->enabled())
            metaTopics_->onJoin(session->sharedInfo());
    }

    void removeSession(const SessionInfo& info)
    {
        auto sid = info.sessionId();
        auto found = sessions_.find(sid);
        if (found == sessions_.end())
            return;

        {
            const MutexGuard guard(sessionQueryMutex_);
            sessions_.erase(found);
        }

        metaTopics_->inhibitSession(sid);
        broker_.removeSubscriber(info);
        dealer_.removeSession(info);
        if (metaTopics_->enabled())
            metaTopics_->onLeave(info);
        metaTopics_->clearSessionInhibitions();
    }

    void closeRealm(Reason r)
    {
        SessionMap sessions;

        {
            const MutexGuard guard{sessionQueryMutex_};
            sessions = std::move(sessions_);
            sessions_.clear();
        }

        std::string msg = "Shutting down realm with reason " + r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        log({LogLevel::info, std::move(msg)});

        for (auto& kv: sessions)
            kv.second->abort(r);
        isOpen_.store(false);
        if (metaTopics_->enabled())
            metaTopics_->onRealmClosed(config_.uri());
    }

    bool doKillSession(SessionId sid, Reason reason)
    {
        auto iter = sessions_.find(sid);
        if (iter == sessions_.end())
            return false;

        auto session = iter->second;
        session->abort(std::move(reason));
        // session->abort will call RouterRealm::leave,
        // which will remove the session from the sessions_ map

        return true;
    }

    void doKillSessions(const SessionIdSet& set, const Reason& reason)
    {
        for (auto sid: set)
        {
            auto found = sessions_.find(sid);
            if (found != sessions_.end())
            {
                auto session = found->second;
                session->abort(reason);
                // session->abort will call RouterRealm::leave,
                // which will remove the session from the sessions_ map
            }
        }
    }

    template <typename F>
    std::vector<SessionId> doKillSessionIf(F&& filter, const Reason& reason)
    {
        std::vector<SessionId> killedIds;
        std::vector<RouterSession::Ptr> killedSessions;

        // Cannot abort sessions as we traverse, as it would invalidate
        // iterators.
        for (auto& kv: sessions_)
        {
            auto& session = kv.second;
            if (filter(session->sharedInfo()))
            {
                killedIds.push_back(session->wampId());
                killedSessions.push_back(session);
            }
        }

        for (const auto& session: killedSessions)
        {
            session->abort(reason);
            // session->abort will call RouterRealm::leave,
            // which will remove the session from the sessions_ map
        }

        return killedIds;
    }

    void leave(const RouterSession::Ptr& session)
    {
        struct Dispatched
        {
            Ptr self;
            SessionInfo info;
            void operator()() {self->removeSession(info);}
        };

        if (!session->isJoined())
            return;
        safelyDispatch<Dispatched>(session->sharedInfo());
    }

    template <typename C>
    void processAuthorization(RouterSession::Ptr originator, C&& command,
                              Authorization auth)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
            C command;
            Authorization a;

            void operator()()
            {
                self->onAuthorized(std::move(s), std::move(command), a);
            }
        };

        boost::asio::dispatch(
            strand(), Dispatched{shared_from_this(), std::move(originator),
                       std::forward<C>(command), auth});
    }

    void send(RouterSession::Ptr originator, Topic&& topic)
    {
        originator->report(topic.info());

        if (topic.matchPolicy() == MatchPolicy::unknown)
        {
            auto error =
                Error::fromRequest({}, topic, WampErrc::optionNotAllowed)
                    .withArgs("unknown match option");
            return originator->sendRouterCommand(std::move(error), true);
        }

        const bool isPattern = topic.matchPolicy() != MatchPolicy::exact;
        if (!uriValidator_->checkTopic(topic.uri(), isPattern))
            return originator->abort({WampErrc::invalidUri});

        authorize(std::move(originator), std::move(topic));
    }

    void onAuthorized(const RouterSession::Ptr& originator, Topic&& topic,
                      Authorization auth)
    {
        if (!checkAuthorization(*originator, topic, auth))
            return;

        auto rid = topic.requestId({});
        auto uri = topic.uri();

        auto subId = broker_.subscribe(originator, std::move(topic));
        if (!subId)
            return;

        Subscribed ack{rid, *subId};
        originator->sendRouterCommand(std::move(ack), std::move(uri));
    }

    void send(RouterSession::Ptr originator, Unsubscribe&& cmd)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
            Unsubscribe u;

            void operator()()
            {
                auto& me = *self;
                auto rid = u.requestId({});
                auto topic = me.broker_.unsubscribe(s, u.subscriptionId());
                if (!me.checkResult<Unsubscribe>(topic, *s, rid))
                    return;
                Unsubscribed ack{rid};
                s->sendRouterCommand(std::move(ack), std::move(*topic));
            }
        };

        originator->report(cmd.info());
        safelyDispatch<Dispatched>(std::move(originator), std::move(cmd));
    }

    void send(RouterSession::Ptr originator, Pub&& pub)
    {
        originator->report(pub.info());

        if (!uriValidator_->checkTopic(pub.uri(), false))
            return originator->abort({WampErrc::invalidUri});

        authorize(std::move(originator), std::move(pub));
    }

    void onAuthorized(const RouterSession::Ptr& originator, Pub&& pub,
                      Authorization auth)
    {
        auto uri = pub.uri();
        const auto rid = pub.requestId({});
        const bool wantsAck = pub.optionOr<bool>("acknowledge", false);

        if (!checkAuthorization(*originator, pub, auth))
            return;
        auto realmRule = config_.publisherDisclosure();
        if (!setDisclosed(*originator, pub, auth, realmRule, wantsAck))
            return;

        auto pubIdAndCount = broker_.publish(originator, std::move(pub));

        Published ack{rid, pubIdAndCount.first};
        if (wantsAck)
        {
            originator->sendRouterCommand(std::move(ack), std::move(uri),
                                          pubIdAndCount.second);
        }
        else
        {
            originator->report(ack.info(std::move(uri), pubIdAndCount.second));
        }
    }

    void send(RouterSession::Ptr originator, Procedure&& proc)
    {
        originator->report(proc.info());

        if (proc.matchPolicy() != MatchPolicy::exact)
        {
            auto error =
                Error::fromRequest({}, proc, WampErrc::optionNotAllowed)
                    .withArgs("pattern-based registrations not supported");
            return originator->sendRouterCommand(std::move(error), true);
        }

        if (!uriValidator_->checkTopic(proc.uri(), false))
            return originator->abort({WampErrc::invalidUri});

        authorize(std::move(originator), std::move(proc));
    }

    void onAuthorized(const RouterSession::Ptr& originator, Procedure&& proc,
                      Authorization auth)
    {
        if (!checkAuthorization(*originator, proc, auth))
            return;

        auto rid = proc.requestId({});
        auto uri = proc.uri();
        auto regId = dealer_.enroll(originator, std::move(proc));
        if (!checkResult<Procedure>(regId, *originator, rid))
            return;
        Registered ack{rid, *regId};
        originator->sendRouterCommand(std::move(ack), std::move(uri));
    }

    void send(RouterSession::Ptr originator, Unregister&& cmd)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
            Unregister u;

            void operator()()
            {
                auto& me = *self;
                auto rid = u.requestId({});
                auto uri = me.dealer_.unregister(s, u.registrationId());
                if (!me.checkResult<Unregister>(uri, *s, rid))
                    return;
                Unregistered ack{rid};
                s->sendRouterCommand(std::move(ack), std::move(*uri));
            }
        };

        originator->report(cmd.info());
        safelyDispatch<Dispatched>(std::move(originator), cmd);
    }

    void send(RouterSession::Ptr originator, Rpc&& rpc)
    {
        originator->report(rpc.info());

        if (!uriValidator_->checkProcedure(rpc.uri(), false))
            return originator->abort({WampErrc::invalidUri});

        if (!checkProcedureExistsBeforeAuthorizing(*originator, rpc))
            return;

        authorize(std::move(originator), std::move(rpc));
    }

    bool checkProcedureExistsBeforeAuthorizing(RouterSession& originator,
                                               const Rpc& rpc)
    {
        if (!config_.authorizer())
            return true;

        bool found = dealer_.hasProcedure(rpc.uri());
        if (!found && (metaProcedures_ != nullptr))
            found = metaProcedures_->hasProcedure(rpc.uri());

        if (!found)
        {
            originator.sendRouterCommand(
                Error::fromRequest({}, rpc, WampErrc::noSuchProcedure), true);
        }

        return found;
    }

    void onAuthorized(const RouterSession::Ptr& originator, Rpc&& rpc,
                      Authorization auth)
    {
        if (!checkAuthorization(*originator, rpc, auth))
            return;
        if (!setDisclosed(*originator, rpc, auth, config_.callerDisclosure()))
            return;
        auto done = dealer_.call(originator, rpc);

        bool isMetaProcedure = false;
        auto unex = makeUnexpectedError(WampErrc::noSuchProcedure);
        auto rid = rpc.requestId({});
        if (metaProcedures_ && (done == unex))
            isMetaProcedure = metaProcedures_->call(*originator, std::move(rpc));

        // A result or error would have already been sent to the caller
        // if it was a valid meta-procedure.
        if (!isMetaProcedure)
            checkResult<Rpc>(done, *originator, rid);
    }

    void send(RouterSession::Ptr originator, CallCancellation&& cancel)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
            CallCancellation c;

            void operator()()
            {
                auto rid = c.requestId({});
                auto done = self->dealer_.cancelCall(s, std::move(c));
                self->checkResult<CallCancellation>(done, *s, rid);
            }
        };

        originator->report(cancel.info());
        safelyDispatch<Dispatched>(std::move(originator), std::move(cancel));
    }

    void send(RouterSession::Ptr originator, Result&& result)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
            Result r;
            void operator()() {self->dealer_.yieldResult(s, std::move(r));}
        };

        originator->report(result.info(false));
        safelyDispatch<Dispatched>(std::move(originator), std::move(result));
    }

    void send(RouterSession::Ptr originator, Error&& error)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
            Error e;
            void operator()() {self->dealer_.yieldError(s, std::move(e));}
        };

        originator->report(error.info(false));

        if (!uriValidator_->checkError(error.uri()))
            return originator->abort({WampErrc::invalidUri});

        safelyDispatch<Dispatched>(std::move(originator), std::move(error));
    }

    template <typename C>
    void authorize(RouterSession::Ptr s, C&& command)
    {
        const auto& authorizer = config_.authorizer();
        if (!authorizer)
        {
            return onAuthorized(std::move(s), std::forward<C>(command), true);
        }

        AuthorizationRequest r{{}, RealmContext{shared_from_this()}, s};
        authorizer->authorize(std::forward<C>(command), std::move(r),
                              executor_);
    }

    template <typename C>
    bool checkAuthorization(RouterSession& originator, const C& command,
                            Authorization auth)
    {
        if (auth.good())
            return true;

        auto ec = make_error_code(WampErrc::authorizationDenied);
        bool isKnownAuthError = true;

        if (auth.error())
        {
            isKnownAuthError =
                auth.error() == WampErrc::authorizationDenied ||
                auth.error() == WampErrc::authorizationFailed ||
                auth.error() == WampErrc::authorizationRequired ||
                auth.error() == WampErrc::discloseMeDisallowed;

            ec = isKnownAuthError ?
                     auth.error() :
                     make_error_code(WampErrc::authorizationFailed);
        }

        auto error = Error::fromRequest({}, command, ec);
        if (!isKnownAuthError)
        {
            error.withArgs(briefErrorCodeString(auth.error()),
                           auth.error().message());
        }

        originator.sendRouterCommand(std::move(error), true);
        return false;
    }

    template <typename C>
    bool setDisclosed(RouterSession& originator, C& command,
                      const Authorization& auth, DisclosureRule realmRule,
                      bool wantsAck = true)
    {
        using DR = DisclosureRule;

        auto authRule = auth.disclosure();
        auto rule = (authRule == DR::preset) ? realmRule : authRule;
        bool disclosed = command.discloseMe();
        const bool isStrict = rule == DR::strictConceal ||
                              rule == DR::strictReveal;

        if (disclosed && isStrict)
        {
            auto error = Error::fromRequest(
                {}, command, make_error_code(WampErrc::discloseMeDisallowed));
            if (wantsAck)
                originator.sendRouterCommand(std::move(error), true);
            return false;
        }

        if (rule == DR::conceal || rule == DR::strictConceal)
            disclosed = false;
        if (rule == DR::reveal || rule == DR::strictReveal)
            disclosed = true;
        command.setDisclosed({}, disclosed);
        return true;
    }

    template <typename TCommand, typename T>
    bool checkResult(const ErrorOr<T>& result, RouterSession& originator,
                     RequestId reqId, bool logOnly = false)
    {
        if (result)
            return true;

        if (result == makeUnexpectedError(WampErrc::protocolViolation))
            return false; // ABORT should already have been sent to originator

        Error error{PassKey{}, TCommand::messageKind({}), reqId,
                    result.error()};
        if (logOnly)
            originator.report(error.info(true));
        else
            originator.sendRouterCommand(std::move(error), true);
        return false;
    }

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand(), F{shared_from_this(), std::forward<Ts>(args)...});
    }

    void publishMetaEvent(Pub&& pub, SessionId inhibitedSessionId) override
    {
        broker_.publishMetaEvent(std::move(pub), inhibitedSessionId);
    }

    mutable std::mutex sessionQueryMutex_;
    AnyIoExecutor executor_;
    IoStrand strand_;
    RealmConfig config_;
    RouterContext router_;
    SessionMap sessions_;
    MetaTopics::Ptr metaTopics_;
    Broker broker_;
    Dealer dealer_;
    std::string logSuffix_;
    RouterLogger::Ptr logger_;
    UriValidator::Ptr uriValidator_;
    RealmProceduresPtr metaProcedures_;
    std::atomic<bool> isOpen_;

    friend class DirectPeer;
    friend class RealmContext;
    template <typename> friend class MetaProcedures;
};


//******************************************************************************
// RealmContext
//******************************************************************************

inline RealmContext::RealmContext(const std::shared_ptr<RouterRealm>& r)
    : realm_(r)
{}

inline bool RealmContext::expired() const {return realm_.expired();}

inline RouterLogger::Ptr RealmContext::logger() const
{
    auto r = realm_.lock();
    if (r)
        return r->logger();
    return {};
}

inline void RealmContext::reset() {realm_.reset();}

inline bool RealmContext::join(RouterSessionPtr session)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->join(std::move(session));
    return true;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
inline bool RealmContext::leave(const RouterSessionPtr& session) noexcept
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->leave(session);
    return true;
}

template <typename C>
bool RealmContext::send(RouterSessionPtr originator, C&& command)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->send(std::move(originator), std::forward<C>(command));
    return true;
}

template <typename C>
bool RealmContext::processAuthorization(RouterSessionPtr originator,
                                        C&& command, Authorization auth)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->processAuthorization(std::move(originator), std::forward<C>(command),
                            auth);
    return true;
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
