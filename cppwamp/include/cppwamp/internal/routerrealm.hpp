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
#include "../routerconfig.hpp"
#include "broker.hpp"
#include "dealer.hpp"
#include "random.hpp"
#include "realmsession.hpp"
#include "routercontext.hpp"

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
        c.initialize({});
        return Ptr(new RouterRealm(std::move(e), std::move(c), rcfg,
                                   std::move(rctx)));
    }

    const IoStrand& strand() const {return strand_;}

    const std::string& uri() const {return config_.uri();}

    void join(RealmSession::Ptr session)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr session;

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
    template <typename TOperation, typename TCommand>
    struct AuthorizationHandler
    {
        WeakPtr self;
        RealmSession::WeakPtr s;
        TOperation op;

        void operator()(Authorization a, any anyCommand)
        {
            auto me = self.lock();
            if (!me)
                return;
            auto originator = s.lock();
            if (!originator)
                return;
            auto& command = *any_cast<TCommand>(&anyCommand);
            me->onAuthorization(std::move(a), std::move(command),
                                std::move(originator), std::move(op));
        }
    };

    template <typename TOperation, typename TCommand>
    void onAuthorization(Authorization&& a, TCommand&& c, RealmSession::Ptr s,
                         TOperation&& op)
    {
        struct Dispatched
        {
            Ptr self;
            Authorization authorization;
            TCommand command;
            RealmSession::Ptr originator;
            TOperation op;

            void operator()()
            {
                auto rid = command.requestId({});
                auto kind = TCommand::messageKind({});
                if (!checkAuthorization(authorization, *originator, kind, rid))
                    return;
                if (authorization.hasTrustLevel())
                    command.setTrustLevel({}, authorization.trustLevel());
                op(std::move(self), std::move(originator),
                   authorization.disclosure(), std::move(command));
            }
        };

        safelyDispatch<Dispatched>(std::move(a), std::move(c), std::move(s),
                                   std::move(op));
    }

    static bool checkAuthorization(const Authorization& auth, RealmSession& s,
                                   MessageKind reqKind, RequestId rid,
                                   bool logOnly = false)
    {
        if (auth.error())
        {
            if (auth.error() == WampErrc::authorizationDenied ||
                auth.error() == WampErrc::authorizationFailed ||
                auth.error() == WampErrc::authorizationRequired ||
                auth.error() == WampErrc::discloseMeDisallowed)
            {
                Error error{{}, reqKind, rid, auth.error()};
                s.sendError(std::move(error), logOnly);
            }
            else
            {
                auto ec = make_error_code(WampErrc::authorizationFailed);
                auto error = Error({}, reqKind, rid, ec)
                                 .withArgs(briefErrorCodeString(auth.error()),
                                           auth.error().message());
                s.sendError(std::move(error), logOnly);
            }
            return false;
        }
        else if (!auth.allowed())
        {
            s.sendError(reqKind, rid, WampErrc::authorizationDenied, logOnly);
            return false;
        }

        return true;
    }

    template <typename TOperation, typename TCommand>
    void dispatchAuthorized(AuthorizationAction action,
                            RealmSession::Ptr originator, TCommand&& command)
    {
        if (config_.authorizer())
        {
            dispatchDynamicallyAuthorized<TOperation>(
                action, std::move(originator), std::move(command));
        }
        else
        {
            dispatchDirectly<TOperation>(std::move(originator),
                                         std::move(command));
        }
    }

    template <typename TOperation, typename TCommand>
    void dispatchDynamicallyAuthorized(AuthorizationAction action,
                                       RealmSession::Ptr originator,
                                       TCommand&& command)
    {
        struct Dispatched
        {
            Ptr self;
            AuthorizationRequest request;
            void operator()() {self->config_.authorizer()(std::move(request));}
        };

        auto self = shared_from_this();
        const auto& authorizer = config_.authorizer();
        AuthorizationHandler<TOperation, TCommand> handler{self, originator,
                                                           TOperation{}};

        AuthorizationRequest req{{}, action, std::move(command),
                                 originator->sharedAuthInfo(),
                                 std::move(handler)};

        auto exec =
            boost::asio::get_associated_executor(authorizer, strand_);

        Dispatched dispatched{std::move(self), std::move(req)};

        dispatchAny(
            strand_,
            boost::asio::bind_executor(exec, std::move(dispatched)));
    }

    template <typename TOperation, typename TCommand>
    void dispatchDirectly(RealmSession::Ptr originator, TCommand&& command)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr originator;
            TCommand command;
            TOperation op;

            void operator()()
            {
                op(std::move(self), std::move(originator),
                   DisclosureRule::preset, std::move(command));
            }
        };

        safelyDispatch<Dispatched>(std::move(originator),
                                   std::forward<TCommand>(command));
    }

    template <typename TData>
    static std::error_code setDisclosed(TData& data, DisclosureRule realmRule,
                                        DisclosureRule authenticatorRule)
    {
        using DR = DisclosureRule;

        auto rule = (authenticatorRule == DR::preset) ? realmRule
                                                      : authenticatorRule;

        bool disclosed = data.discloseMe();

        bool isStrict = rule == DR::strictConceal || rule == DR::strictReveal;
        if (disclosed && isStrict)
            return make_error_code(WampErrc::discloseMeDisallowed);

        if (rule == DR::conceal || rule == DR::strictConceal)
            disclosed = false;
        if (rule == DR::reveal || rule == DR::strictReveal)
            disclosed = true;
        data.setDisclosed({}, disclosed);
        return {};
    }

    RouterRealm(Executor&& e, RealmConfig&& c, const RouterConfig& rcfg,
                RouterContext&& rctx)
        : strand_(boost::asio::make_strand(e)),
          config_(std::move(c)),
          router_(std::move(rctx)),
          broker_(rcfg.publicationRNG(), config_.topicUriValidator()),
          dealer_(strand_, config_.procedureUriValidator()),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          logger_(router_.logger())
    {}

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void report(RealmSession& session, AccessActionInfo&& action)
    {
        session.report(std::move(action), *logger_);
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand(), F{shared_from_this(), std::forward<Ts>(args)...});
    }

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

    void send(RealmSession::Ptr s, Topic&& topic)
    {
        struct Dispatched
        {
            void operator()(Ptr self, RealmSession::Ptr s, DisclosureRule,
                            Topic&& t)
            {
                auto rid = t.requestId({});
                auto result = self->broker_.subscribe(s, std::move(t));
                if (result)
                    s->sendSubscribed(Subscribed{rid, *result});
                else
                    s->sendError(MessageKind::subscribe, rid, result);
            }
        };

        report(*s, topic.info());
        dispatchAuthorized<Dispatched>(AuthorizationAction::subscribe,
                                       std::move(s), std::move(topic));
    }

    void send(RealmSession::Ptr s, Unsubscribe&& cmd)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr s;
            Unsubscribe u;

            void operator()()
            {
                auto rid = u.requestId({});
                auto result = self->broker_.unsubscribe(s, u.subscriptionId());
                if (result)
                    s->sendUnsubscribed(Unsubscribed{rid}, std::move(*result));
                else
                    s->sendError(MessageKind::unsubscribe, rid, result);
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(cmd));
    }

    void send(RealmSession::Ptr s, Pub&& pub)
    {
        struct Dispatched
        {
            void operator()(Ptr self, RealmSession::Ptr s, DisclosureRule rule,
                            Pub&& p)
            {
                auto rid = p.requestId({});
                bool ack = p.optionOr<bool>("acknowledge", false);
                auto realmRule = self->config_.publisherDisclosure();
                auto ec = setDisclosed(p, realmRule, rule);
                if (ec)
                    return s->sendError(MessageKind::publish, rid, ec, !ack);

                auto result = self->broker_.publish(s, std::move(p));
                if (result)
                {
                    if (ack)
                        s->sendPublished(Published{rid, *result});
                }
                else
                {
                    s->sendError(MessageKind::publish, rid, result, !ack);
                }
            }
        };

        dispatchAuthorized<Dispatched>(
            AuthorizationAction::publish, std::move(s), std::move(pub));
    }

    void send(RealmSession::Ptr s, Procedure&& proc)
    {
        struct Dispatched
        {
            void operator()(Ptr self, RealmSession::Ptr s, DisclosureRule rule,
                            Procedure&& p)
            {
                auto rid = p.requestId({});
                auto result = self->dealer_.enroll(s, std::move(p));
                if (result)
                    s->sendRegistered(Registered{rid, *result});
                else
                    s->sendError(MessageKind::enroll, rid, result);
            }
        };

        dispatchAuthorized<Dispatched>(
            AuthorizationAction::enroll, std::move(s), std::move(proc));
    }

    void send(RealmSession::Ptr s, Unregister&& cmd)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr s;
            Unregister u;

            void operator()()
            {
                auto rid = u.requestId({});
                auto result = self->dealer_.unregister(s, u.registrationId());
                if (result)
                    s->sendUnregistered(Unregistered{rid}, std::move(*result));
                else
                    s->sendError(MessageKind::unregister, rid, result);
            }
        };

        safelyDispatch<Dispatched>(std::move(s), cmd);
    }

    void send(RealmSession::Ptr s, Rpc&& rpc)
    {
        struct Dispatched
        {
            void operator()(Ptr self, RealmSession::Ptr s, DisclosureRule rule,
                            Rpc&& r)
            {
                auto rid = r.requestId({});
                auto realmRule = self->config_.callerDisclosure();
                auto ec = setDisclosed(r, realmRule, rule);
                if (ec)
                    s->sendError(MessageKind::call, rid, ec);
                auto result = self->dealer_.call(s, std::move(r));
                if (!result)
                    s->sendError(MessageKind::call, rid, result);
            }
        };

        dispatchAuthorized<Dispatched>(
            AuthorizationAction::enroll, std::move(s), std::move(rpc));
    }

    void send(RealmSession::Ptr s, CallCancellation&& c)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr s;
            CallCancellation c;

            void operator()()
            {
                auto rid = c.requestId();
                auto result = self->dealer_.cancelCall(s, std::move(c));
                if (!result)
                    s->sendError(MessageKind::call, rid, result);
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(c));
    }

    void send(RealmSession::Ptr s, Result&& r)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr s;
            Result r;

            void operator()()
            {
                self->dealer_.yieldResult(std::move(s), std::move(r));
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(r));
    }

    void send(RealmSession::Ptr s, Error&& e)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr s;
            Error e;

            void operator()()
            {
                self->dealer_.yieldError(std::move(s), std::move(e));
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(e));
    }

    IoStrand strand_;
    RealmConfig config_;
    RouterContext router_;
    std::map<SessionId, RealmSession::Ptr> sessions_;
    Broker broker_;
    Dealer dealer_;
    std::string logSuffix_;
    RouterLogger::Ptr logger_;

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

inline bool RealmContext::join(RealmSessionPtr s)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->join(std::move(s));
    return true;
}

inline bool RealmContext::leave(SessionId sid)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->leave(sid);
    return true;
}

template <typename TCommand>
bool RealmContext::send(RealmSessionPtr s, TCommand&& cmd)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    r->send(std::move(s), std::forward<TCommand>(cmd));
    return true;
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
