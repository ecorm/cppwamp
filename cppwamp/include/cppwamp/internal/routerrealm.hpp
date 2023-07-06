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
        return dealer_->getRegistration(rid, listCallees);
    }

    ErrorOr<RegistrationInfo> lookupRegistration(const Uri& uri, MatchPolicy p,
                                                 bool listCallees = false)
    {
        return dealer_->lookupRegistration(uri, p, listCallees);
    }

    ErrorOr<RegistrationInfo> bestRegistrationMatch(const Uri& uri,
                                                    bool listCallees = false)
    {
        return dealer_->bestRegistrationMatch(uri, listCallees);
    }

    template <typename F>
    std::size_t forEachRegistration(MatchPolicy p, F&& functor) const
    {
        return dealer_->forEachRegistration(p, std::forward<F>(functor));
    }

    ErrorOr<SubscriptionInfo> getSubscription(SubscriptionId sid,
                                              bool listSubscribers = false)
    {
        return broker_->getSubscription(sid, listSubscribers);
    }

    ErrorOr<SubscriptionInfo> lookupSubscription(const Uri& uri, MatchPolicy p,
                                                 bool listSubscribers = false)
    {
        return broker_->lookupSubscription(uri, p, listSubscribers);
    }

    template <typename F>
    std::size_t forEachSubscription(MatchPolicy p, F&& functor) const
    {
        return broker_->forEachSubscription(p, std::forward<F>(functor));
    }

    template <typename F>
    std::size_t forEachMatchingSubscription(const Uri& uri, F&& functor) const
    {
        return broker_->forEachMatch(uri, std::forward<F>(functor));
    }

    RouterRealm(const RouterRealm&) = delete;
    RouterRealm(RouterRealm&&) = delete;
    RouterRealm& operator=(const RouterRealm&) = delete;
    RouterRealm& operator=(RouterRealm&&) = delete;

private:
    using SessionMap = std::map<SessionId, RouterSession::Ptr>;
    using RealmProcedures = RealmMetaProcedures<RouterRealm>;
    using RealmProceduresPtr = RealmMetaProcedures<RouterRealm>::Ptr;
    using MutexGuard = std::lock_guard<std::mutex>;

    template <typename C, typename... Ts>
    static void sendCommandErrorToOriginator(
        RouterSession& originator, const C& cmd, WampErrc errc, Ts&&... args)
    {
        auto error = Error::fromRequest({}, cmd, errc)
                         .withArgs(std::forward<Ts>(args)...);
        originator.sendRouterCommand(std::move(error), true);
    }

    RouterRealm(Executor&& e, RealmConfig&& c, const RouterConfig& rcfg,
                RouterContext&& rctx, RandomNumberGenerator64&& rng)
        : executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          config_(std::move(c)),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          router_(std::move(rctx)),
          metaTopics_(std::make_shared<MetaTopics>(this, executor_, strand_,
                                                   config_.metaApiEnabled())),
          broker_(std::make_shared<Broker>(executor_, strand_, std::move(rng),
                                           metaTopics_, config_)),
          dealer_(std::make_shared<Dealer>(executor_, strand_, metaProcedures_,
                                           metaTopics_, config_)),
          logger_(router_.logger()),
          uriValidator_(rcfg.uriValidator()),
          isOpen_(true)
    {
        if (config_.metaApiEnabled())
            metaProcedures_ = std::make_shared<RealmProcedures>(this);
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
        broker_->removeSubscriber(info);
        dealer_->removeSession(info);
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

    void send(RouterSession::Ptr originator, Topic&& subscribe)
    {
        originator->report(subscribe.info());

        const bool isPattern = subscribe.matchPolicy() != MatchPolicy::exact;
        if (!uriValidator_->checkTopic(subscribe.uri(), isPattern))
            return originator->abort({WampErrc::invalidUri});

        broker_->dispatchCommand(originator, std::move(subscribe));
    }

    void send(RouterSession::Ptr originator, Unsubscribe&& cmd)
    {
        originator->report(cmd.info());
        broker_->dispatchCommand(originator, std::move(cmd));
    }

    void send(RouterSession::Ptr originator, Pub&& publish)
    {
        originator->report(publish.info());

        if (!uriValidator_->checkTopic(publish.uri(), false))
            return originator->abort({WampErrc::invalidUri});

        broker_->dispatchCommand(originator, std::move(publish));
    }

    void send(RouterSession::Ptr originator, Procedure&& enroll)
    {
        originator->report(enroll.info());

        dealer_->dispatchCommand(originator, std::move(enroll));
    }

    void send(RouterSession::Ptr originator, Unregister&& cmd)
    {
        originator->report(cmd.info());
        dealer_->dispatchCommand(originator, std::move(cmd));
    }

    void send(RouterSession::Ptr originator, Rpc&& call)
    {
        originator->report(call.info());

        if (!uriValidator_->checkProcedure(call.uri(), false))
            return originator->abort({WampErrc::invalidUri});

        dealer_->dispatchCommand(originator, std::move(call));
    }

    void send(RouterSession::Ptr originator, CallCancellation&& cancel)
    {
        originator->report(cancel.info());
        dealer_->dispatchCommand(originator, std::move(cancel));
    }

    void send(RouterSession::Ptr originator, Result&& yielded)
    {
        originator->report(yielded.info(false));
        dealer_->dispatchCommand(originator, std::move(yielded));
    }

    void send(RouterSession::Ptr originator, Error&& yielded)
    {
        originator->report(yielded.info(false));

        if (!uriValidator_->checkError(yielded.uri()))
            return originator->abort({WampErrc::invalidUri});

        dealer_->dispatchCommand(originator, std::move(yielded));
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
        broker_->publishMetaEvent(std::move(pub), inhibitedSessionId);
    }

    mutable std::mutex sessionQueryMutex_;
    AnyIoExecutor executor_;
    IoStrand strand_;
    RealmConfig config_;
    SessionMap sessions_;
    std::string logSuffix_;
    RouterContext router_;
    MetaTopics::Ptr metaTopics_;
    Broker::Ptr broker_;
    Dealer::Ptr dealer_;
    RouterLogger::Ptr logger_;
    UriValidator::Ptr uriValidator_;
    RealmProceduresPtr metaProcedures_;
    std::atomic<bool> isOpen_;

    friend class DirectPeer;
    friend class RealmContext;
    template <typename> friend class RealmMetaProcedures;
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

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
