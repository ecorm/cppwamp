/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_METAAPI_HPP
#define CPPWAMP_INTERNAL_METAAPI_HPP

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>
#include "../anyhandler.hpp"
#include "../pubsubinfo.hpp"
#include "../realmobserver.hpp"
#include "../rpcinfo.hpp"
#include "cppwamp/asiodefs.hpp"
#include "matchpolicyoption.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class MetaProcedures
{
public:
    using Ptr = std::shared_ptr<MetaProcedures>;

    virtual ~MetaProcedures() = default;

    virtual bool hasProcedure(const Uri& uri) const = 0;

    virtual bool call(RouterSession& caller, Rpc&& rpc) = 0;
};

//------------------------------------------------------------------------------
template <typename TContext>
class RealmMetaProcedures : public MetaProcedures
{
public:
    using Context = TContext;
    using Ptr = std::shared_ptr<RealmMetaProcedures>;

    explicit RealmMetaProcedures(Context* realm) :
        handlers_(
        {{
            {"wamp.registration.count_callees",     &Self::countRegistrationCallees},
            {"wamp.registration.get",               &Self::registrationDetails},
            {"wamp.registration.list",              &Self::listRegistrations},
            {"wamp.registration.list_callees",      &Self::listRegistrationCallees},
            {"wamp.registration.lookup",            &Self::lookupRegistration},
            {"wamp.registration.match",             &Self::matchRegistration},
            {"wamp.session.count",                  &Self::sessionCount},
            {"wamp.session.get",                    &Self::sessionDetails},
            {"wamp.session.kill",                   &Self::killSession},
            {"wamp.session.kill_all",               &Self::killAllSessions},
            {"wamp.session.kill_by_authid",         &Self::killSessionsByAuthId},
            {"wamp.session.kill_by_authrole",       &Self::killSessionsByAuthRole},
            {"wamp.session.list",                   &Self::sessionList},
            {"wamp.subscription.count_subscribers", &Self::countSubscribers},
            {"wamp.subscription.get",               &Self::subscriptionDetails},
            {"wamp.subscription.list",              &Self::listSubscriptions},
            {"wamp.subscription.list_subscribers",  &Self::listSubscribers},
            {"wamp.subscription.lookup",            &Self::lookupSubscription},
            {"wamp.subscription.match",             &Self::matchSubscriptions}
        }}),
        context_(realm)
    {}

    bool hasProcedure(const Uri& uri) const override
    {
        auto iter = std::lower_bound(handlers_.cbegin(), handlers_.cend(), uri);
        return (iter != handlers_.cend() && (iter->uri == uri));
    }

    bool call(RouterSession& caller, Rpc&& rpc) override
    {
        auto iter = std::lower_bound(handlers_.cbegin(), handlers_.cend(),
                                     rpc.uri());
        if (iter == handlers_.cend() || (iter->uri != rpc.uri()))
            return false;

        auto requestId = rpc.requestId({});
        auto handler = iter->handler;

        Outcome outcome;
        try
        {
            outcome = (this->*(handler))(caller, rpc);
        }
        catch (Error& e)
        {
            outcome = std::move(e);
        }
        catch (const error::BadType& e)
        {
            outcome = Error{e};
        }

        if (outcome.type() == Outcome::Type::result)
        {
            Result result{std::move(outcome).asResult()};
            result.setRequestId({}, requestId);
            caller.sendRouterCommand(std::move(result), true);
        }
        else
        {
            assert(outcome.type() == Outcome::Type::error);
            Error error{std::move(outcome).asError()};
            error.setRequestId({}, rpc.requestId({}));
            caller.sendRouterCommand(std::move(error), true);
        }

        return true;
    }

private:
    using Self = RealmMetaProcedures;
    using Handler = Outcome (RealmMetaProcedures::*)(RouterSession&, Rpc&);

    struct Entry
    {
        const char* uri;
        Handler handler;
    };
    friend bool operator<(const Entry& e, const Uri& uri) {return e.uri < uri;}
    friend bool operator<(const Uri& uri, const Entry& e) {return uri < e.uri;}

    static std::set<std::string> parseAuthRoles(const Rpc& rpc)
    {
        Array authRoleArray;
        rpc.convertTo(authRoleArray);
        std::set<std::string> authRoles;
        for (auto& elem: authRoleArray)
            authRoles.emplace(std::move(elem.as<String>()));
        return authRoles;
    }

    static Reason parseReason(Rpc& rpc)
    {
        auto unex = makeUnexpectedError(MiscErrc::badType);

        auto reasonArg = std::move(rpc).kwargAs<String>("reason");
        if (reasonArg == unex)
            throw error::Conversion{"'reason' argument must be a string"};

        // NOLINTNEXTLINE(bugprone-use-after-move)
        auto messageArg = std::move(rpc).kwargAs<String>("message");
        if (messageArg == unex)
            throw error::Conversion{"'message' argument must be a string"};

        String reasonUri{errorCodeToUri(WampErrc::sessionKilled)};
        if (reasonArg && !reasonArg->empty())
            reasonUri = std::move(*reasonArg);
        Reason reason{std::move(reasonUri)};
        if (messageArg && !messageArg->empty())
            reason.withHint(std::move(*messageArg));
        return reason;
    }

    static MatchPolicy parseMatchPolicy(const Rpc& rpc)
    {
        if (rpc.args().size() < 2)
            return MatchPolicy::exact;
        const auto& optionsArg = rpc.args()[1];
        if (!optionsArg.is<Object>())
            throw error::Conversion{"second argument must be an object"};

        const auto& dict = optionsArg.as<Object>();
        return getMatchPolicyOption(dict);
    }

    Outcome sessionCount(RouterSession&, Rpc& rpc)
    {
        std::size_t count = 0;
        if (rpc.args().empty())
        {
            count = context_->sessionCount();
        }
        else
        {
            auto authRoles = parseAuthRoles(rpc);
            context_->forEachSession(
                [&authRoles, &count](const SessionInfo& info) -> bool
                {
                    const auto& role = info.auth().role();
                    count += authRoles.count(role);
                    return true;
                });
        }
        return Result{count};
    }

    Outcome sessionList(RouterSession&, Rpc& rpc)
    {
        std::vector<SessionId> list;

        if (rpc.args().empty())
        {
            context_->forEachSession(
                [&list](const SessionInfo& info) -> bool
                {
                    list.push_back(info.sessionId());
                    return true;
                });
        }
        else
        {
            auto authRoles = parseAuthRoles(rpc);
            context_->forEachSession(
                [&authRoles, &list](const SessionInfo& info) -> bool
                {
                    const auto& role = info.auth().role();
                    if (authRoles.count(role) != 0)
                        list.push_back(info.sessionId());
                    return true;
                });
        }

        return Result{std::move(list)};
    }

    Outcome sessionDetails(RouterSession&, Rpc& rpc)
    {
        SessionId sid = 0;
        rpc.convertTo(sid);
        auto details = context_->getSession(sid);
        if (!details)
            return Error{WampErrc::noSuchSession};
        return Result{toObject(details)};
    }

    Outcome killSession(RouterSession& caller, Rpc& rpc)
    {
        SessionId sid = 0;
        rpc.convertTo(sid);
        if (sid == caller.wampId())
            return Error{WampErrc::noSuchSession};

        auto reason = parseReason(rpc);
        auto killed = context_->doKillSession(sid, std::move(reason));
        if (!killed)
            return Error{WampErrc::noSuchSession};
        return Result{};
    }

    template <typename TFilter>
    std::vector<SessionId> killSessions(Rpc& rpc, TFilter&& filter)
    {
        auto reason = parseReason(rpc);
        return context_->doKillSessionIf(filter, reason);
    }

    Outcome killSessionsByAuthId(RouterSession& caller, Rpc& rpc)
    {
        String authId;
        auto ownId = caller.wampId();
        rpc.convertTo(authId);
        auto killed = killSessions(
            rpc,
            [&authId, ownId](const SessionInfo& info) -> bool
            {
                auto sid = info.sessionId();
                return (sid != ownId) && (info.auth().id() == authId);
            });
        return Result{std::move(killed)};
    }

    Outcome killSessionsByAuthRole(RouterSession& caller, Rpc& rpc)
    {
        String authRole;
        auto ownId = caller.wampId();
        rpc.convertTo(authRole);
        auto killed = killSessions(
            rpc,
            [&authRole, ownId](const SessionInfo& info) -> bool
            {
                auto sid = info.sessionId();
                return (sid != ownId) && (info.auth().role() == authRole);
            });
        return Result{killed.size()};
    }

    Outcome killAllSessions(RouterSession& caller, Rpc& rpc)
    {
        auto ownId = caller.wampId();
        auto killed = killSessions(
            rpc,
            [ownId](const SessionInfo& info) -> bool
            {
                return info.sessionId() != ownId;
            });
        return Result{killed.size()};
    }

    Outcome listRegistrations(RouterSession&, Rpc&)
    {
        std::vector<RegistrationId> exact;
        std::vector<RegistrationId> prefix;
        std::vector<RegistrationId> wildcard;

        context_->forEachRegistration(
            MatchPolicy::exact,
            [&exact](const RegistrationInfo& s) -> bool
            {
                exact.push_back(s.id);
                return true;
            });

        context_->forEachRegistration(
            MatchPolicy::prefix,
            [&prefix](const RegistrationInfo& s) -> bool
            {
                prefix.push_back(s.id);
                return true;
            });

        context_->forEachRegistration(
            MatchPolicy::wildcard,
            [&wildcard](const RegistrationInfo& s) -> bool
            {
                wildcard.push_back(s.id);
                return true;
            });

        return Result{Object{
            {"exact", std::move(exact)},
            {"prefix", std::move(prefix)},
            {"wildcard", std::move(wildcard)}}};
    }

    Outcome lookupRegistration(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);

        auto policy = parseMatchPolicy(rpc);
        if (policy == MatchPolicy::unknown)
            return Result{null};

        auto info = context_->lookupRegistration(uri, policy);
        return info ? Result{info->id} : Result{null};
    }

    Outcome matchRegistration(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);
        auto info = context_->bestRegistrationMatch(uri);
        return info ? Result{info->id} : Result{null};
    }

    Outcome registrationDetails(RouterSession&, Rpc& rpc)
    {
        RegistrationId rid = 0;
        rpc.convertTo(rid);
        auto info = context_->getRegistration(rid);
        return info ? Result{Variant::from(*info)} : Result{null};
    }

    Outcome listRegistrationCallees(RouterSession&, Rpc& rpc)
    {
        RegistrationId rid = 0;
        rpc.convertTo(rid);
        auto info = context_->getRegistration(rid, true);
        if (!info)
            return Error{WampErrc::noSuchRegistration};
        Array list;
        for (auto callee: info->callees)
            list.push_back(callee);
        return Result{std::move(list)};
    }

    Outcome countRegistrationCallees(RouterSession&, Rpc& rpc)
    {
        RegistrationId rid = 0;
        rpc.convertTo(rid);
        auto info = context_->getRegistration(rid);
        if (!info)
            return Error{WampErrc::noSuchRegistration};
        return Result{info->calleeCount};
    }

    Outcome listSubscriptions(RouterSession&, Rpc&)
    {
        std::vector<SubscriptionId> exact;
        std::vector<SubscriptionId> prefix;
        std::vector<SubscriptionId> wildcard;

        context_->forEachSubscription(
            MatchPolicy::exact,
            [&exact](const SubscriptionInfo& s) -> bool
            {
                exact.push_back(s.id);
                return true;
            });

        context_->forEachSubscription(
            MatchPolicy::prefix,
            [&prefix](const SubscriptionInfo& s) -> bool
            {
                prefix.push_back(s.id);
                return true;
            });

        context_->forEachSubscription(
            MatchPolicy::wildcard,
            [&wildcard](const SubscriptionInfo& s) -> bool
            {
                wildcard.push_back(s.id);
                return true;
            });

        return Result{Object{
            {"exact", std::move(exact)},
            {"prefix", std::move(prefix)},
            {"wildcard", std::move(wildcard)}}};
    }

    Outcome lookupSubscription(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);

        auto policy = parseMatchPolicy(rpc);
        if (policy == MatchPolicy::unknown)
            return Result{null};

        auto info = context_->lookupSubscription(uri, policy);
        return info ? Result{info->id} : Result{null};
    }

    Outcome matchSubscriptions(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);
        std::vector<SubscriptionId> list;
        context_->forEachMatchingSubscription(
            uri,
            [&list](const SubscriptionInfo& s) -> bool
            {
                list.push_back(s.id);
                return true;
            });
        return Result{std::move(list)};
    }

    Outcome subscriptionDetails(RouterSession&, Rpc& rpc)
    {
        SubscriptionId sid = 0;
        rpc.convertTo(sid);
        auto info = context_->getSubscription(sid);
        return info ? Result{Variant::from(*info)} : Result{null};
    }

    Outcome listSubscribers(RouterSession&, Rpc& rpc)
    {
        SubscriptionId sid = 0;
        rpc.convertTo(sid);
        auto info = context_->getSubscription(sid, true);
        if (!info)
            return Error{WampErrc::noSuchSubscription};
        Array list;
        for (auto subscriber: info->subscribers)
            list.push_back(subscriber);
        return Result{std::move(list)};
    }

    Outcome countSubscribers(RouterSession&, Rpc& rpc)
    {
        SubscriptionId sid = 0;
        rpc.convertTo(sid);
        auto info = context_->getSubscription(sid);
        if (!info)
            return Error{WampErrc::noSuchSubscription};
        return Result{info->subscriberCount};
    }

    static constexpr unsigned handlerCount_ = 19;
    std::array<Entry, handlerCount_> handlers_;
    Context* context_ = nullptr;
};

//------------------------------------------------------------------------------
class MetaPublisher
{
public:
    virtual ~MetaPublisher() = default;
    virtual void publishMetaEvent(Pub&&, SessionId) = 0;
};

//------------------------------------------------------------------------------
class MetaTopics : public RealmObserver
{
public:
    using Ptr = std::shared_ptr<MetaTopics>;
    using WeakPtr = std::weak_ptr<MetaTopics>;
    using Executor = AnyIoExecutor;
    using FallbackExecutor = AnyCompletionExecutor;
    using ObserverId = uint64_t;

    MetaTopics(MetaPublisher* realm, Executor executor, IoStrand strand,
               bool metaApiEnabled)
        : executor_(std::move(executor)),
          strand_(std::move(strand)),
          context_(realm),
          metaApiEnabled_(metaApiEnabled)
    {}

    bool enabled() const {return metaApiEnabled_ || !observers_.empty();}

    void addObserver(const RealmObserver::Ptr& o, const FallbackExecutor& e)
    {
        const WeakPtr self =
            std::static_pointer_cast<MetaTopics>(shared_from_this());
        auto id = ++nextObserverId_;
        o->attach(self, id, e);
        observers_.emplace(id, o);
    }

    void inhibitSession(SessionId sid) {inhibitedSessionId_ = sid;}

    void clearSessionInhibitions() {inhibitedSessionId_ = nullId();}

    void onRealmClosed(const Uri& uri) override
    {
        struct Notifier
        {
            RealmObserver::WeakPtr observer;
            Uri uri;

            void operator()() const
            {
                auto o = observer.lock();
                if (o)
                    o->onRealmClosed(uri);
            }
        };

        if (!observers_.empty())
            notifyObservers<Notifier>(uri);
    }
    
    void onJoin(const SessionInfo& info) override
    {
        struct Notifier
        {
            RealmObserver::WeakPtr observer;
            SessionInfo s;

            void operator()() const
            {
                auto o = observer.lock();
                if (o)
                    o->onJoin(s);
            }
        };

        if (metaApiEnabled_)
            publish(Pub{"wamp.session.on_join"}.withArgs(toObject(info)));

        if (!observers_.empty())
            notifyObservers<Notifier>(info);
    }
    
    void onLeave(const SessionInfo& info) override
    {
        struct Notifier
        {
            RealmObserver::WeakPtr observer;
            SessionInfo s;

            void operator()() const
            {
                auto o = observer.lock();
                if (o)
                    o->onLeave(s);
            }
        };

        if (metaApiEnabled_)
        {
            publish(Pub{"wamp.session.on_leave"}
                        .withArgs(info.sessionId(),
                                  info.auth().id(),
                                  info.auth().role()));
        }

        if (!observers_.empty())
            notifyObservers<Notifier>(info);
    }
    
    void onRegister(const SessionInfo& info,
                    const RegistrationInfo& reg) override
    {
        struct Notifier
        {
            RealmObserver::WeakPtr observer;
            SessionInfo s;
            RegistrationInfo r;

            void operator()() const
            {
                auto o = observer.lock();
                if (o)
                    o->onRegister(s, r);
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = info.sessionId();

            if (reg.calleeCount == 1u)
            {
                publish(Pub{"wamp.registration.on_create"}
                            .withArgs(sid, Variant::from(reg)));
            }

            publish(Pub{"wamp.registration.on_register"}.withArgs(sid, reg.id));
        }

        if (!observers_.empty())
            notifyObservers<Notifier>(info, reg);
    }
    
    void onUnregister(const SessionInfo& info,
                      const RegistrationInfo& reg) override
    {
        struct Notifier
        {
            RealmObserver::WeakPtr observer;
            SessionInfo s;
            RegistrationInfo r;

            void operator()() const
            {
                auto o = observer.lock();
                if (o)
                    o->onUnregister(s, r);
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = info.sessionId();
            publish(Pub{"wamp.registration.on_unregister"}.withArgs(sid, reg.id));

            if (reg.calleeCount == 0)
                publish(Pub{"wamp.registration.on_delete"}.withArgs(sid, reg.id));
        }

        if (!observers_.empty())
            notifyObservers<Notifier>(info, reg);
    }
    
    void onSubscribe(const SessionInfo& info,
                     const SubscriptionInfo& sub) override
    {
        struct Notifier
        {
            RealmObserver::WeakPtr observer;
            SessionInfo s;
            SubscriptionInfo sub;

            void operator()() const
            {
                auto o = observer.lock();
                if (o)
                    o->onSubscribe(s, sub);
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = info.sessionId();

            if (sub.subscriberCount == 1)
            {
                publish(Pub{"wamp.subscription.on_create"}
                            .withArgs(sid, Variant::from(sub)));
            }

            publish(Pub{"wamp.subscription.on_subscribe"}
                        .withArgs(sid, sub.id));
        }

        if (!observers_.empty())
            notifyObservers<Notifier>(info, sub);
    }
    
    void onUnsubscribe(const SessionInfo& info,
                       const SubscriptionInfo& sub) override
    {
        struct Notifier
        {
            RealmObserver::WeakPtr observer;
            SessionInfo s;
            SubscriptionInfo sub;

            void operator()() const
            {
                auto o = observer.lock();
                if (o)
                    o->onUnsubscribe(s, sub);
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = info.sessionId();
            publish(Pub{"wamp.subscription.on_unsubscribe"}
                        .withArgs(sid, sub.id));

            if (sub.subscriberCount == 0)
            {
                publish(Pub{"wamp.subscription.on_delete"}
                            .withArgs(sid, sub.id));
            }
        }

        if (!observers_.empty())
            notifyObservers<Notifier>(info, sub);
    }

private:
    void onDetach(ObserverId id) override
    {
        auto self = std::static_pointer_cast<MetaTopics>(shared_from_this());
        boost::asio::dispatch(strand_,
                              [id, self]() {self->observers_.erase(id);});
    }

    void publish(Pub& pub)
    {
        context_->publishMetaEvent(std::move(pub), inhibitedSessionId_);
    }

    template <typename TNotifier, typename... Ts>
    void notifyObservers(Ts&&... args)
    {
        TNotifier notifier{{}, std::forward<Ts>(args)...};
        for (auto& kv: observers_)
        {
            auto observer = kv.second.lock();
            if (observer)
            {
                notifier.observer = observer;
                observer->notify(executor_, notifier);
            }
        }
    }

    Executor executor_;
    IoStrand strand_;
    std::map<ObserverId, RealmObserver::WeakPtr> observers_;
    MetaPublisher* context_ = nullptr;
    ObserverId nextObserverId_ = 0;
    SessionId inhibitedSessionId_ = nullId();
    bool metaApiEnabled_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_METAAPI_HPP
