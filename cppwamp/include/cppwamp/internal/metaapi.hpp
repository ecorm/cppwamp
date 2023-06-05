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
template <typename TContext>
class MetaProcedures
{
public:
    using Context = TContext;

    MetaProcedures(Context* realm) :
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
        context_(*realm)
    {}

    bool call(RouterSession& caller, Rpc&& rpc)
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
    using Self = MetaProcedures;
    typedef Outcome (MetaProcedures::*Handler)(RouterSession&, Rpc&);

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
        {
            throw Error{WampErrc::invalidArgument}
                .withArgs("'reason' argument must be a string");
        }

        auto messageArg = std::move(rpc).kwargAs<String>("message");
        if (messageArg == unex)
        {
            throw Error{WampErrc::invalidArgument}
                .withArgs("'message' argument must be a string");
        }

        String reasonUri{errorCodeToUri(WampErrc::sessionKilled)};
        if (reasonArg)
            reasonUri = std::move(*reasonArg);
        if (reasonUri.empty())
        {
            throw Error{WampErrc::invalidUri}
                .withArgs("'reason' argument cannot be empty");
        }
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
        {
            throw Error{WampErrc::invalidArgument}
                .withArgs("second argument must be an object");
        }

        const auto& dict = optionsArg.as<Object>();
        return getMatchPolicyOption(dict);
    }

    Outcome sessionCount(RouterSession&, Rpc& rpc)
    {
        if (rpc.args().empty())
            return Result{context_.sessionCount(nullptr)};

        auto authRoles = parseAuthRoles(rpc);
        auto filter =
            [&authRoles](SessionDetails session) -> bool
            {
                const auto& role = session.authInfo.role();
                return authRoles.count(role) != 0;
            };
        return Result{context_.sessionCount(filter)};
    }

    Outcome sessionList(RouterSession&, Rpc& rpc)
    {
        if (rpc.args().empty())
            return Result{context_.sessionList(nullptr)};

        auto authRoles = parseAuthRoles(rpc);
        auto filter =
            [&authRoles](SessionDetails session) -> bool
            {
                const auto& role = session.authInfo.role();
                return authRoles.count(role) != 0;
            };

        return Result{context_.sessionList(filter)};
    }

    Outcome sessionDetails(RouterSession&, Rpc& rpc)
    {
        SessionId sid = 0;
        rpc.convertTo(sid);
        auto details = context_.sessionDetails(sid);
        if (!details)
            return Error{details.error()};
        return Result{toObject(*details)};
    }

    Outcome killSession(RouterSession& caller, Rpc& rpc)
    {
        SessionId sid = 0;
        rpc.convertTo(sid);
        if (sid == caller.wampId())
            return Error{WampErrc::noSuchSession};

        auto reason = parseReason(rpc);
        auto killed = context_.doKillSession(sid, std::move(reason));
        if (!killed)
            return Error{killed.error()};
        return Result{};
    }

    template <typename TFilter>
    std::vector<SessionId> killSessions(Rpc& rpc, TFilter&& filter)
    {
        auto reason = parseReason(rpc);
        return context_.doKillSessions(filter, reason);
    }

    Outcome killSessionsByAuthId(RouterSession& caller, Rpc& rpc)
    {
        String authId;
        auto ownId = caller.wampId();
        rpc.convertTo(authId);
        auto killed = killSessions(
            rpc,
            [&authId, ownId](SessionDetails s) -> bool
            {
                auto sid = s.authInfo.sessionId();
                return (sid != ownId) && (s.authInfo.id() == authId);
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
            [&authRole, ownId](SessionDetails s) -> bool
            {
                auto sid = s.authInfo.sessionId();
                return (sid != ownId) && (s.authInfo.role() == authRole);
            });
        return Result{killed.size()};
    }

    Outcome killAllSessions(RouterSession& caller, Rpc& rpc)
    {
        auto ownId = caller.wampId();
        auto killed = killSessions(
            rpc,
            [ownId](SessionDetails s) -> bool
            {
                return s.authInfo.sessionId() != ownId;
            });
        return Result{killed.size()};
    }

    Outcome listRegistrations(RouterSession&, Rpc& rpc)
    {
        auto lists = context_.registrationLists();
        return Result{toObject(context_.registrationLists())};
    }

    Outcome lookupRegistration(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);

        auto policy = parseMatchPolicy(rpc);
        if (policy == MatchPolicy::unknown)
            return Result{null};

        auto details = context_.registrationDetailsByUri(uri, policy);
        return details ? Result{details->info.id} : Result{null};
    }

    Outcome matchRegistration(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);
        auto match = context_.bestRegistrationMatch(uri);
        return match ? Result{match->info.id} : Result{null};
    }

    Outcome registrationDetails(RouterSession&, Rpc& rpc)
    {
        RegistrationId rid;
        rpc.convertTo(rid);
        auto details = context_.registrationDetailsById(rid);
        return details ? Result{toObject(*details)} : Result{null};
    }

    Outcome listRegistrationCallees(RouterSession&, Rpc& rpc)
    {
        RegistrationId rid;
        rpc.convertTo(rid);
        auto details = context_.registrationDetailsById(rid);
        if (!details)
            return Error{WampErrc::noSuchRegistration};
        return Result{details->callees};
    }

    Outcome countRegistrationCallees(RouterSession&, Rpc& rpc)
    {
        RegistrationId rid;
        rpc.convertTo(rid);
        auto details = context_.registrationDetailsById(rid);
        if (!details)
            return Error{WampErrc::noSuchRegistration};
        return Result{details->callees.size()};
    }

    Outcome listSubscriptions(RouterSession&, Rpc& rpc)
    {
        auto lists = context_.subscriptionLists();
        return Result{toObject(context_.subscriptionLists())};
    }

    Outcome lookupSubscription(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);

        auto policy = parseMatchPolicy(rpc);
        if (policy == MatchPolicy::unknown)
            return Result{null};

        auto details = context_.subscriptionDetailsByUri(uri, policy);
        return details ? Result{details->info.id} : Result{null};
    }

    Outcome matchSubscriptions(RouterSession&, Rpc& rpc)
    {
        Uri uri;
        rpc.convertTo(uri);
        return Result{context_.subscriptionMatches(uri)};
    }

    Outcome subscriptionDetails(RouterSession&, Rpc& rpc)
    {
        SubscriptionId rid;
        rpc.convertTo(rid);
        auto details = context_.subscriptionDetailsById(rid);
        return details ? Result{toObject(*details)} : Result{null};
    }

    Outcome listSubscribers(RouterSession&, Rpc& rpc)
    {
        SubscriptionId rid;
        rpc.convertTo(rid);
        auto details = context_.subscriptionDetailsById(rid);
        if (!details)
            return Error{WampErrc::noSuchSubscription};
        return Result{details->subscribers};
    }

    Outcome countSubscribers(RouterSession&, Rpc& rpc)
    {
        SubscriptionId rid;
        rpc.convertTo(rid);
        auto details = context_.subscriptionDetailsById(rid);
        if (!details)
            return Error{WampErrc::noSuchSubscription};
        return Result{details->subscribers.size()};
    }

    std::array<Entry, 19> handlers_;
    Context& context_;
};

//------------------------------------------------------------------------------
class MetaPublisher
{
public:
    virtual void publishMetaEvent(Pub&& pub) = 0;
};

//------------------------------------------------------------------------------
class MetaTopics : public RealmObserver
{
public:
    using Ptr = std::shared_ptr<MetaTopics>;
    using WeakPtr = std::weak_ptr<MetaTopics>;
    using Executor = AnyIoExecutor;
    using ObserverExecutor = AnyCompletionExecutor;
    using ObserverId = uint64_t;

    MetaTopics(MetaPublisher* realm, Executor executor, IoStrand strand,
               bool metaApiEnabled)
        : executor_(executor),
          strand_(std::move(strand)),
          context_(*realm),
          metaApiEnabled_(metaApiEnabled)
    {}

    bool enabled() const {return metaApiEnabled_ || !observers_.empty();}

    void addObserver(RealmObserver::Ptr o, ObserverExecutor e)
    {
        WeakPtr self = std::static_pointer_cast<MetaTopics>(shared_from_this());
        auto id = ++nextObserverId_;
        o->attach(
            {},
            [self, id]()
            {
                auto me = self.lock();
                if (me)
                    me->safelyRemoveObserver(id);
            });

        observers_.emplace(id, ObserverRecord{o, std::move(e)});
    }

    void removeObserver(ObserverId id)
    {
        observers_.erase(id);
    }

    void onRealmClosed(Uri uri) override
    {
        struct Posted
        {
            RealmObserver::WeakPtr observer;
            Uri uri;

            void operator()()
            {
                auto o = observer.lock();
                if (o)
                    o->onRealmClosed(uri);
            }
        };

        if (!observers_.empty())
            postToAllObservers<Posted>(std::move(uri));
    }

    void onJoin(SessionDetails s) override
    {
        struct Posted
        {
            RealmObserver::WeakPtr observer;
            SessionDetails s;

            void operator()()
            {
                auto o = observer.lock();
                if (o)
                    o->onJoin(std::move(s));
            }
        };

        if (metaApiEnabled_)
            publish(Pub{"wamp.session.on_join"}.withArgs(toObject(s)));

        if (!observers_.empty())
            postToAllObservers<Posted>(std::move(s));
    }

    void onLeave(SessionDetails s) override
    {
        struct Posted
        {
            RealmObserver::WeakPtr observer;
            SessionDetails s;

            void operator()()
            {
                auto o = observer.lock();
                if (o)
                    o->onLeave(std::move(s));
            }
        };

        if (metaApiEnabled_)
            publish(Pub{"wamp.session.on_leave"}
                        .withArgs(s.authInfo.sessionId(),
                                  s.authInfo.id(),
                                  s.authInfo.role()));

        if (!observers_.empty())
            postToAllObservers<Posted>(std::move(s));
    }

    void onRegister(SessionDetails s, RegistrationDetails r) override
    {
        struct Posted
        {
            RealmObserver::WeakPtr observer;
            SessionDetails s;
            RegistrationDetails r;

            void operator()()
            {
                auto o = observer.lock();
                if (o)
                    o->onRegister(std::move(s), std::move(r));
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = s.authInfo.sessionId();

            if (r.callees.size() == 1)
            {
                publish(Pub{"wamp.registration.on_create"}
                            .withArgs(sid, toObject(r)));
            }

            publish(Pub{"wamp.registration.on_register"}
                        .withArgs(sid, r.info.id));
        }

        if (!observers_.empty())
            postToAllObservers<Posted>(std::move(s), std::move(r));
    }

    void onUnregister(SessionDetails s, RegistrationDetails r) override
    {
        struct Posted
        {
            RealmObserver::WeakPtr observer;
            SessionDetails s;
            RegistrationDetails r;

            void operator()()
            {
                auto o = observer.lock();
                if (o)
                    o->onUnregister(std::move(s), std::move(r));
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = s.authInfo.sessionId();
            publish(Pub{"wamp.registration.on_unregister"}
                        .withArgs(sid, r.info.id));

            if (r.callees.empty())
            {
                publish(Pub{"wamp.registration.on_delete"}
                            .withArgs(sid, r.info.id));
            }
        }

        if (!observers_.empty())
            postToAllObservers<Posted>(std::move(s), std::move(r));
    }

    void onSubscribe(SessionDetails s, SubscriptionDetails sub) override
    {
        struct Posted
        {
            RealmObserver::WeakPtr observer;
            SessionDetails s;
            SubscriptionDetails sub;

            void operator()()
            {
                auto o = observer.lock();
                if (o)
                    o->onSubscribe(std::move(s), std::move(sub));
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = s.authInfo.sessionId();

            if (sub.subscribers.size() == 1)
            {
                publish(Pub{"wamp.subscription.on_create"}
                            .withArgs(sid, toObject(sub)));
            }

            publish(Pub{"wamp.subscription.on_subscribe"}
                        .withArgs(sid, sub.info.id));
        }

        if (!observers_.empty())
            postToAllObservers<Posted>(std::move(s), std::move(sub));
    }

    void onUnsubscribe(SessionDetails s, SubscriptionDetails sub) override
    {
        struct Posted
        {
            RealmObserver::WeakPtr observer;
            SessionDetails s;
            SubscriptionDetails sub;

            void operator()()
            {
                auto o = observer.lock();
                if (o)
                    o->onUnsubscribe(std::move(s), std::move(sub));
            }
        };

        if (metaApiEnabled_)
        {
            auto sid = s.authInfo.sessionId();
            publish(Pub{"wamp.subscription.on_unsubscribe"}
                        .withArgs(sid, sub.info.id));

            if (sub.subscribers.empty())
            {
                publish(Pub{"wamp.subscription.on_delete"}
                            .withArgs(sid, sub.info.id));
            }
        }

        if (!observers_.empty())
            postToAllObservers<Posted>(std::move(s), std::move(sub));
    }

private:
    struct ObserverRecord
    {
        RealmObserver::WeakPtr observer;
        ObserverExecutor executor;
    };

    void safelyRemoveObserver(ObserverId id)
    {
        auto self = std::static_pointer_cast<MetaTopics>(shared_from_this());
        boost::asio::dispatch(strand_,
                              [id, self]() {self->observers_.erase(id);});
    }

    void publish(Pub& pub) {context_.publishMetaEvent(std::move(pub));}

    template <typename TPosted, typename... Ts>
    void postToAllObservers(Ts&&... args)
    {
        TPosted posted{{}, std::forward<Ts>(args)...};
        for (auto& kv: observers_)
        {
            auto& record = kv.second;
            posted.observer = record.observer;
            boost::asio::post(executor_,
                              bind_executor(record.executor, posted));
        }
    }

    Executor executor_;
    IoStrand strand_;
    std::map<ObserverId, ObserverRecord> observers_;
    MetaPublisher& context_;
    ObserverId nextObserverId_ = 0;
    bool metaApiEnabled_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_METAAPI_HPP
