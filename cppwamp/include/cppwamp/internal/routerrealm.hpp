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
                      RouterContext rctx)
    {
        return Ptr(new RouterRealm(std::move(e), std::move(c), rcfg,
                                   std::move(rctx)));
    }

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
            void operator()() {self->joinSession(std::move(session));}
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

    void observe(RealmObserver::Ptr o, FallbackExecutor e)
    {
        struct Dispatched
        {
            Ptr self;
            RealmObserver::Ptr o;
            FallbackExecutor e;

            void operator()()
            {
                self->metaTopics_->addObserver(std::move(o), std::move(e));
            }
        };

        safelyDispatch<Dispatched>(std::move(o), std::move(e));
    }

    std::size_t sessionCount() const
    {
        std::lock_guard<std::mutex> guard{sessionQueryMutex_};
        return sessions_.size();
    }

    template <typename F>
    std::size_t forEachSession(F&& functor) const
    {
        std::lock_guard<std::mutex> guard{sessionQueryMutex_};
        std::size_t count = 0;
        for (const auto& kv: sessions_)
        {
            if (!std::forward<F>(functor)(kv.second->info()))
                break;
            ++count;
        }
        return count;
    }

    SessionInfo::ConstPtr lookupSession(SessionId sid) const
    {
        MutexGuard guard{sessionQueryMutex_};
        auto found = sessions_.find(sid);
        return (found == sessions_.end()) ? nullptr
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
            MutexGuard guard{sessionQueryMutex_};
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
            MutexGuard guard{sessionQueryMutex_};
            for (const auto& kv: sessions_)
            {
                const auto& session = kv.second;
                if (std::forward<F>(filter)(session->info()))
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
            MutexGuard guard{sessionQueryMutex_};
            auto end = set.end();
            auto iter = set.begin();
            while (iter != end)
            {
                SessionId sid = *iter;
                if (sessions_.count(sid))
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

private:
    using SessionMap = std::map<SessionId, RouterSession::Ptr>;
    using RealmProcedures = MetaProcedures<RouterRealm>;
    using MutexGuard = std::lock_guard<std::mutex>;

    RouterRealm(Executor&& e, RealmConfig&& c, const RouterConfig& rcfg,
                RouterContext&& rctx)
        : executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          config_(std::move(c)),
          router_(std::move(rctx)),
          metaTopics_(std::make_shared<MetaTopics>(this, executor_, strand_,
                                                   config_.metaApiEnabled())),
          broker_(rcfg.publicationRNG(), metaTopics_),
          dealer_(strand_, metaTopics_),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          logger_(router_.logger()),
          uriValidator_(rcfg.uriValidator()),
          isOpen_(true)
    {
        if (config_.metaApiEnabled())
            metaProcedures_.reset(new RealmProcedures(this));
    }

    RouterLogger::Ptr logger() const {return logger_;}

    void joinSession(RouterSession::Ptr session)
    {
        auto reservedId = router_.reserveSessionId();
        auto id = reservedId.get();
        session->setWampId(std::move(reservedId));

        {
            MutexGuard guard{sessionQueryMutex_};
            sessions_.emplace(id, session);
        }

        if (metaTopics_->enabled())
            metaTopics_->onJoin(session->sharedInfo());
    }

    void removeSession(SessionInfo::ConstPtr info)
    {
        auto sid = info->sessionId();
        auto found = sessions_.find(sid);
        if (found == sessions_.end())
            return;

        {
            MutexGuard guard(sessionQueryMutex_);
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
            MutexGuard guard{sessionQueryMutex_};
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
            if (filter(session->info()))
            {
                killedIds.push_back(session->wampId());
                killedSessions.push_back(session);
            }
        }

        for (auto session: killedSessions)
        {
            session->abort(reason);
            // session->abort will call RouterRealm::leave,
            // which will remove the session from the sessions_ map
        }

        return killedIds;
    }

    void leave(RouterSession::Ptr session)
    {
        struct Dispatched
        {
            Ptr self;
            SessionInfo::ConstPtr info;
            void operator()() {self->removeSession(std::move(info));}
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

        bool isPattern = topic.matchPolicy() != MatchPolicy::exact;
        if (!uriValidator_->checkTopic(topic.uri(), isPattern))
            return originator->abort({WampErrc::invalidUri});

        authorize(std::move(originator), std::move(topic));
    }

    void onAuthorized(RouterSession::Ptr originator, Topic&& topic,
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
                if (!me.checkResult(topic, *s, u))
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

    void onAuthorized(RouterSession::Ptr originator, Pub&& pub,
                      Authorization auth)
    {
        auto uri = pub.uri();
        auto rid = pub.requestId({});
        bool wantsAck = pub.optionOr<bool>("acknowledge", false);

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

    void onAuthorized(RouterSession::Ptr originator, Procedure&& proc,
                      Authorization auth)
    {
        if (!checkAuthorization(*originator, proc, auth))
            return;

        auto rid = proc.requestId({});
        auto uri = proc.uri();
        auto regId = dealer_.enroll(originator, std::move(proc));
        if (!checkResult(regId, *originator, proc))
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
                if (!me.checkResult(uri, *s, u))
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

        authorize(std::move(originator), std::move(rpc));
    }

    void onAuthorized(RouterSession::Ptr originator, Rpc&& rpc,
                      Authorization auth)
    {
        if (!checkAuthorization(*originator, rpc, auth))
            return;
        if (!setDisclosed(*originator, rpc, auth, config_.callerDisclosure()))
            return;
        auto done = dealer_.call(originator, rpc);

        bool isMetaProcedure = false;
        auto unex = makeUnexpectedError(WampErrc::noSuchProcedure);
        if (metaProcedures_ && (done == unex))
            isMetaProcedure = metaProcedures_->call(*originator, std::move(rpc));

        // A result or error would have already been sent to the caller
        // if it was a valid meta-procedure.
        if (!isMetaProcedure)
            checkResult(done, *originator, rpc);
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
                auto done = self->dealer_.cancelCall(s, std::move(c));
                self->checkResult(done, *s, c);
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

            void operator()()
            {
                self->dealer_.yieldResult(std::move(s), std::move(r));
            }
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

            void operator()()
            {
                self->dealer_.yieldError(std::move(s), std::move(e));
            }
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

        AuthorizationRequest r{{}, shared_from_this(), s};
        authorizer->authorize(std::forward<C>(command), std::move(r));
    }

    template <typename C>
    bool checkAuthorization(RouterSession& originator, const C& command,
                            Authorization auth)
    {
        if (auth)
            return true;

        auto ec = make_error_code(WampErrc::authorizationDenied);
        bool isKnownAuthError = true;

        if (auth.error())
        {
            bool isKnownAuthError =
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
        bool isStrict = rule == DR::strictConceal || rule == DR::strictReveal;

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

    template <typename T, typename C>
    bool checkResult(const ErrorOr<T>& result, RouterSession& originator,
                     const C& command, bool logOnly = false)
    {
        if (result)
            return true;

        if (result == makeUnexpectedError(WampErrc::protocolViolation))
            return false; // ABORT should already have been sent to originator

        auto error = Error::fromRequest({}, command, result.error());
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
    std::unique_ptr<RealmProcedures> metaProcedures_;
    std::atomic<bool> isOpen_;

    friend class DirectPeer;
    friend class RealmContext;
    template <typename> friend class MetaProcedures;
};


//******************************************************************************
// RealmContext
//******************************************************************************

inline RealmContext::RealmContext(std::shared_ptr<RouterRealm> r)
    : realm_(std::move(r))
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

inline bool RealmContext::leave(RouterSessionPtr session)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->leave(std::move(session));
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
