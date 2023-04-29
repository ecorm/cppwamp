/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include "../authorizer.hpp"
#include "../routerconfig.hpp"
#include "broker.hpp"
#include "commandinfo.hpp"
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
    using Ptr = std::shared_ptr<RouterRealm>;
    using WeakPtr = std::weak_ptr<RouterRealm>;
    using Executor = AnyIoExecutor;

    static Ptr create(Executor e, RealmConfig c, const RouterConfig& rcfg,
                      RouterContext rctx)
    {
        return Ptr(new RouterRealm(std::move(e), std::move(c), rcfg,
                                   std::move(rctx)));
    }

    const IoStrand& strand() const {return strand_;}

    const std::string& uri() const {return config_.uri();}

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
                me.sessions_.emplace(id, std::move(session));
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
            }
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

private:
    RouterRealm(Executor&& e, RealmConfig&& c, const RouterConfig& rcfg,
                RouterContext&& rctx)
        : strand_(boost::asio::make_strand(e)),
          config_(std::move(c)),
          router_(std::move(rctx)),
          broker_(rcfg.publicationRNG()),
          dealer_(strand_),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          logger_(router_.logger()),
          uriValidator_(rcfg.uriValidator())
    {}

    RouterLogger::Ptr logger() const {return logger_;}

    void leave(SessionId sid)
    {
        struct Dispatched
        {
            Ptr self;
            SessionId sid;

            void operator()()
            {
                auto& me = *self;
                me.sessions_.erase(sid);
                me.broker_.removeSubscriber(sid);
                me.dealer_.removeSession(sid);
            }
        };

        safelyDispatch<Dispatched>(sid);
    }

    template <typename C>
    void onAuthorized(ThreadSafe, RouterSession::Ptr originator, C&& command,
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
        if (!checkResult(subId, *originator, topic))
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
                auto rid = u.requestId({});
                auto topic = self->broker_.unsubscribe(s, u.subscriptionId());
                if (!self->checkResult(topic, *s, u))
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
                auto rid = u.requestId({});
                auto uri = self->dealer_.unregister(s, u.registrationId());
                if (!self->checkResult(uri, *s, u))
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
        auto done = dealer_.call(originator, std::move(rpc));
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
            return onAuthorized(threadSafe, std::move(s),
                                std::forward<C>(command), true);
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

    IoStrand strand_;
    RealmConfig config_;
    RouterContext router_;
    std::map<SessionId, RouterSession::Ptr> sessions_;
    Broker broker_;
    Dealer dealer_;
    std::string logSuffix_;
    RouterLogger::Ptr logger_;
    UriValidator::Ptr uriValidator_;

    friend class DirectPeer;
    friend class RealmContext;
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
bool RealmContext::onAuthorized(RouterSessionPtr originator, C&& command,
                                Authorization auth)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->onAuthorized(std::move(originator), std::forward<C>(command), auth);
    return true;
}

template <typename C>
bool RealmContext::onAuthorized(ThreadSafe, RouterSessionPtr s, C&& command,
                                Authorization auth)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->onAuthorized(threadSafe, std::move(s), std::forward<C>(command), auth);
    return true;
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
