/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <atomic>
#include <map>
#include <memory>
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
class RouterRealm : public std::enable_shared_from_this<RouterRealm>
{
public:
    using Ptr                 = std::shared_ptr<RouterRealm>;
    using WeakPtr             = std::weak_ptr<RouterRealm>;
    using Executor            = AnyIoExecutor;
    using ObserverExecutor    = AnyCompletionExecutor;
    using SessionHandler      = std::function<void (SessionDetails)>;
    using SessionFilter       = std::function<bool (SessionDetails)>;
    using RegistrationHandler = std::function<void (RegistrationDetails)>;
    using SubscriptionHandler = std::function<void (SubscriptionDetails)>;

    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void (T)>;

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

            void operator()()
            {
                auto& me = *self;
                auto reservedId = me.router_.reserveSessionId();
                auto id = reservedId.get();
                session->setWampId(std::move(reservedId));
                me.sessions_.emplace(id, session);
                auto observer = me.observer_.lock();
                if (observer)
                    observer->onJoin(session->details());
            }
        };

        safelyDispatch<Dispatched>(std::move(session));
    }

    void close(Reason r)
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;

            void operator()()
            {
                auto& me = *self;
                std::string msg = "Shutting down realm with reason " + r.uri();
                if (!r.options().empty())
                    msg += " " + toString(r.options());
                me.log({LogLevel::info, std::move(msg)});

                for (auto& kv: me.sessions_)
                    kv.second->abort(r);
                me.sessions_.clear();
                me.isOpen_.store(false);
                auto observer = me.observer_.lock();
                if (observer)
                    observer->onRealmClosed(me.config_.uri());
            }
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    void observe(RealmObserver::Ptr o, ObserverExecutor e)
    {
        struct Dispatched
        {
            Ptr self;
            RealmObserver::Ptr o;
            ObserverExecutor e;
            void operator()()
            {
                auto& me = *self;
                if (me.config_.metaApiEnabled())
                    me.metaTopics_->setObserver(std::move(o), std::move(e));
                else
                    me.observer_ = std::move(o);
            }
        };

        safelyDispatch<Dispatched>(std::move(o));
    }

    void unobserve()
    {
        struct Dispatched
        {
            Ptr self;
            void operator()()
            {
                auto& me = *self;
                if (me.config_.metaApiEnabled())
                    me.metaTopics_->setObserver(nullptr, nullptr);
                else
                    me.observer_ = {};
            }
        };

        safelyDispatch<Dispatched>();
    }

    void countSessions(SessionFilter f, CompletionHandler<std::size_t> h)
    {
        struct Dispatched
        {
            Ptr self;
            SessionFilter f;
            CompletionHandler<std::size_t> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.sessionCount(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(f), std::move(h));
    }

    void listSessions(SessionFilter f,
                      CompletionHandler<std::vector<SessionId>> h)
    {
        struct Dispatched
        {
            Ptr self;
            SessionFilter f;
            CompletionHandler<std::vector<SessionId>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.sessionList(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(f), std::move(h));
    }

    void forEachSession(SessionHandler f, CompletionHandler<std::size_t> h)
    {
        struct Dispatched
        {
            Ptr self;
            SessionHandler f;
            CompletionHandler<std::size_t> h;

            void operator()()
            {
                auto& me = *self;
                for (const auto& kv: me.sessions_)
                    f(kv.second->details());
                me.complete(h, me.sessions_.size());
            }
        };

        safelyDispatch<Dispatched>(std::move(f), std::move(h));
    }

    void lookupSession(SessionId sid,
                       CompletionHandler<ErrorOr<SessionDetails>> h)
    {
        struct Dispatched
        {
            Ptr self;
            SessionId sid;
            CompletionHandler<ErrorOr<SessionDetails>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.sessionDetails(sid));
            }
        };

        safelyDispatch<Dispatched>(sid, std::move(h));
    }

    void killSession(SessionId sid, Reason r, CompletionHandler<bool> h)
    {
        struct Dispatched
        {
            Ptr self;
            SessionId sid;
            Reason r;
            CompletionHandler<std::size_t> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.doKillSession(sid, std::move(r)));
            }
        };

        safelyDispatch<Dispatched>(sid, std::move(r), std::move(h));
    }

    void killSessions(SessionFilter f, Reason r,
                      CompletionHandler<std::vector<SessionId>> h)
    {
        struct Dispatched
        {
            Ptr self;
            SessionFilter f;
            Reason r;
            CompletionHandler<std::vector<SessionId>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.doKillSessions(std::move(f), r));
            }
        };

        safelyDispatch<Dispatched>(std::move(f), std::move(r), std::move(h));
    }

    void listRegistrations(CompletionHandler<RegistrationLists> h)
    {
        struct Dispatched
        {
            Ptr self;
            CompletionHandler<RegistrationLists> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.registrationLists());
            }
        };

        safelyDispatch<Dispatched>(std::move(h));
    }

    void forEachRegistration(MatchPolicy p, RegistrationHandler f,
                             CompletionHandler<std::size_t> h)
    {
        struct Dispatched
        {
            Ptr self;
            MatchPolicy p;
            RegistrationHandler f;
            CompletionHandler<std::size_t> h;

            void operator()()
            {
                auto& me = *self;
                auto count = me.dealer_.forEachRegistration(p, f);
                me.complete(h, count);
            }
        };

        safelyDispatch<Dispatched>(p, std::move(f), std::move(h));
    }

    void lookupRegistration(Uri uri, MatchPolicy p,
                            CompletionHandler<ErrorOr<RegistrationDetails>> h)
    {
        struct Dispatched
        {
            Ptr self;
            Uri uri;
            MatchPolicy p;
            CompletionHandler<ErrorOr<RegistrationDetails>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.registrationDetailsByUri(uri, p));
            }
        };

        safelyDispatch<Dispatched>(std::move(uri), p, std::move(h));
    }

    void matchRegistration(Uri uri,
                           CompletionHandler<ErrorOr<RegistrationDetails>> h)
    {
        struct Dispatched
        {
            Ptr self;
            Uri uri;
            CompletionHandler<ErrorOr<RegistrationDetails>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.bestRegistrationMatch(uri));
            }
        };

        safelyDispatch<Dispatched>(std::move(uri), std::move(h));
    }

    void getRegistration(RegistrationId rid,
                         CompletionHandler<ErrorOr<RegistrationDetails>> h)
    {
        struct Dispatched
        {
            Ptr self;
            RegistrationId rid;
            CompletionHandler<ErrorOr<RegistrationDetails>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.registrationDetailsById(rid));
            }
        };

        safelyDispatch<Dispatched>(rid, std::move(h));
    }

    void listSubscriptions(CompletionHandler<SubscriptionLists> h)
    {
        struct Dispatched
        {
            Ptr self;
            CompletionHandler<SubscriptionLists> h;

            void operator()()
            {
                auto& me = *self;
                auto lists = me.broker_.listSubscriptions();
                me.complete(h, std::move(lists));
            }
        };

        safelyDispatch<Dispatched>(std::move(h));
    }

    void forEachSubscription(MatchPolicy p, SubscriptionHandler f,
                             CompletionHandler<std::size_t> h)
    {
        struct Dispatched
        {
            Ptr self;
            MatchPolicy p;
            SubscriptionHandler f;
            CompletionHandler<std::size_t> h;

            void operator()()
            {
                auto& me = *self;
                auto count = me.broker_.forEachSubscription(p, f);
                me.complete(h, count);
            }
        };

        safelyDispatch<Dispatched>(p, std::move(f), std::move(h));
    }

    void lookupSubscription(Uri uri, MatchPolicy p,
                            CompletionHandler<ErrorOr<SubscriptionDetails>> h)
    {
        struct Dispatched
        {
            Ptr self;
            Uri uri;
            MatchPolicy p;
            CompletionHandler<ErrorOr<SubscriptionDetails>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.subscriptionDetailsByUri(uri, p));
            }
        };

        safelyDispatch<Dispatched>(std::move(uri), p, std::move(h));
    }

    void matchSubscriptions(Uri uri,
                            CompletionHandler<std::vector<SubscriptionId>> h)
    {
        struct Dispatched
        {
            Ptr self;
            Uri uri;
            CompletionHandler<std::vector<SubscriptionId>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.subscriptionMatches(uri));
            }
        };

        safelyDispatch<Dispatched>(std::move(uri), std::move(h));
    }

    void getSubscription(SubscriptionId sid,
                         CompletionHandler<ErrorOr<SubscriptionDetails>> h)
    {
        struct Dispatched
        {
            Ptr self;
            SubscriptionId sid;
            CompletionHandler<ErrorOr<SubscriptionDetails>> h;

            void operator()()
            {
                auto& me = *self;
                me.complete(h, me.subscriptionDetailsById(sid));
            }
        };

        safelyDispatch<Dispatched>(sid, std::move(h));
    }

private:
    using RealmProcedures = MetaProcedures<RouterRealm>;
    using RealmTopics = MetaTopics<RouterRealm>;

    RouterRealm(Executor&& e, RealmConfig&& c, const RouterConfig& rcfg,
                RouterContext&& rctx)
        : executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          config_(std::move(c)),
          router_(std::move(rctx)),
          broker_(rcfg.publicationRNG()),
          dealer_(strand_),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          logger_(router_.logger()),
          uriValidator_(rcfg.uriValidator()),
          isOpen_(true)
    {
        if (config_.metaApiEnabled())
        {
            metaProcedures_.reset(new RealmProcedures(this));
            metaTopics_ = std::make_shared<RealmTopics>(this, executor_);
            observer_ = metaTopics_;
        }
    }

    RouterLogger::Ptr logger() const {return logger_;}

    std::size_t sessionCount(const SessionFilter& filter) const
    {
        if (!filter)
            return sessions_.size();

        std::size_t count = 0;
        for (const auto& kv: sessions_)
            count += filter(kv.second->details()) ? 1 : 0;
        return count;
    }

    std::vector<SessionId> sessionList(const SessionFilter& filter) const
    {
        std::vector<SessionId> idList;
        for (const auto& kv: sessions_)
        {
            if ((filter == nullptr) || filter(kv.second->details()))
                idList.push_back(kv.first);
        }
        return idList;
    }

    ErrorOr<SessionDetails> sessionDetails(SessionId sid) const
    {
        static constexpr auto errc = WampErrc::noSuchSession;
        auto found = sessions_.find(sid);
        if (found == sessions_.end())
            return makeUnexpectedError(errc);
        else
            return found->second->details();
    }

    bool doKillSession(SessionId sid, Reason reason)
    {
        auto iter = sessions_.find(sid);
        bool found = iter != sessions_.end();
        if (found)
        {
            auto session = iter->second;
            session->abort(std::move(reason));
            // session->abort will call RouterRealm::leave,
            // which will remove the session from the session_ map
        }
        return found;
    }

    std::vector<SessionId> doKillSessions(const SessionFilter& filter,
                                          const Reason& reason)
    {
        std::vector<RouterSession::Ptr> killed;
        std::vector<SessionId> killedIds;

        for (auto& kv: sessions_)
        {
            if (filter(kv.second->details()))
            {
                killed.push_back(kv.second);
                killedIds.push_back(kv.first);
                // Cannot abort the session now as it would remove itself
                // from the sessions_ map and invalidate iterators.
            }
        }

        for (auto& session: killed)
            session->abort(reason);

        return killedIds;
    }

    RegistrationLists registrationLists() const
    {
        return dealer_.listRegistrations();
    }

    ErrorOr<RegistrationDetails> registrationDetailsByUri(const Uri& uri,
                                                          MatchPolicy p) const
    {
        return dealer_.lookupRegistration(uri, p);
    }

    ErrorOr<RegistrationDetails> bestRegistrationMatch(const Uri& uri) const
    {
        return dealer_.matchRegistration(uri);
    }

    ErrorOr<RegistrationDetails>
    registrationDetailsById(RegistrationId rid) const
    {
        return dealer_.getRegistration(rid);
    }

    SubscriptionLists subscriptionLists() const
    {
        return broker_.listSubscriptions();
    }

    ErrorOr<SubscriptionDetails> subscriptionDetailsByUri(const Uri& uri,
                                                          MatchPolicy p) const
    {
        return broker_.lookupSubscription(uri, p);
    }

    std::vector<SubscriptionId> subscriptionMatches(const Uri& uri)
    {
        return broker_.matchSubscriptions(uri);
    }

    ErrorOr<SubscriptionDetails> subscriptionDetailsById(SubscriptionId sid)
    {
        return broker_.getSubscription(sid);
    }

    void leave(SessionId sid)
    {
        struct Dispatched
        {
            Ptr self;
            SessionId sid;

            void operator()()
            {
                auto& me = *self;
                auto found = me.sessions_.find(sid);
                if (found == me.sessions_.end())
                    return;
                auto session = found->second;
                me.sessions_.erase(found);
                me.broker_.removeSubscriber(sid);
                me.dealer_.removeSession(sid);
                auto observer = me.observer_.lock();
                if (observer)
                    observer->onLeave(session->details());
            }
        };

        safelyDispatch<Dispatched>(sid);
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

        auto subId = broker_.subscribe(originator, std::move(topic),
                                       observer_.lock());
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
                auto topic = me.broker_.unsubscribe(s, u.subscriptionId(),
                                                    self->observer_.lock());
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
        auto regId = dealer_.enroll(originator, std::move(proc),
                                    observer_.lock());
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
                auto uri = me.dealer_.unregister(s, u.registrationId(),
                                                 me.observer_.lock());
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

    template <typename S, typename... Ts>
    void complete(AnyCompletionHandler<S>& f, Ts&&... args)
    {
        postAny(executor_, std::move(f), std::forward<Ts>(args)...);
    }

    void publishMetaEvent(Pub&& pub) {broker_.publishMetaEvent(std::move(pub));}

    AnyIoExecutor executor_;
    IoStrand strand_;
    RealmConfig config_;
    RouterContext router_;
    std::map<SessionId, RouterSession::Ptr> sessions_;
    Broker broker_;
    Dealer dealer_;
    std::string logSuffix_;
    RouterLogger::Ptr logger_;
    UriValidator::Ptr uriValidator_;
    RealmObserver::WeakPtr observer_;
    std::unique_ptr<RealmProcedures> metaProcedures_;
    std::shared_ptr<RealmTopics> metaTopics_;
    std::atomic<bool> isOpen_;

    friend class DirectPeer;
    friend class RealmContext;
    template <typename> friend class MetaProcedures;
    template <typename> friend class MetaTopics;
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

inline bool RealmContext::leave(SessionId sessionId)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->leave(sessionId);
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
