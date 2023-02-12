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
                session->setWampId({}, std::move(reservedId));
                me.sessions_.emplace(id, std::move(session));
            }
        };

        safelyDispatch<Dispatched>(std::move(session));
    }

    void close(bool terminate, Reason r)
    {
        struct Dispatched
        {
            Ptr self;
            bool terminate;
            Reason r;

            void operator()()
            {
                auto& me = *self;
                std::string msg = terminate ? "Shutting down realm with reason "
                                            : "Terminating realm with reason ";
                msg += r.uri();
                if (!r.options().empty())
                    msg += " " + toString(r.options());
                me.log({LogLevel::info, std::move(msg)});

                for (auto& kv: me.sessions_)
                    kv.second->close(terminate, r);
                me.sessions_.clear();
            }
        };

        safelyDispatch<Dispatched>(terminate, std::move(r));
    }

private:
    template <typename TOperation, typename TData>
    struct AuthorizationHandler
    {
        WeakPtr self;
        RealmSession::WeakPtr s;
        TOperation op;

        void operator()(Authorization a, any anyData)
        {
            struct Dispatched
            {
                Ptr self;
                Authorization authorization;
                TData data;
                RealmSession::Ptr originator;
                TOperation op;

                void operator()()
                {
                    op(std::move(self), std::move(originator),
                       std::move(authorization), std::move(data));
                }
            };

            auto me = self.lock();
            if (!me)
                return;
            auto originator = s.lock();
            if (!originator)
                return;
            auto& data = *any_cast<TData>(&anyData);
            me->safelyDispatch<Dispatched>(std::move(a), std::move(data),
                                           std::move(originator),
                                           std::move(op));
        }
    };

    static bool checkAuthorization(const Authorization& auth, RealmSession& s,
                                   WampMsgType reqType, RequestId rid,
                                   bool logOnly = false)
    {
        if (auth.error())
        {
            auto ec = make_error_code(SessionErrc::authorizationFailed);
            std::ostringstream oss;
            oss << ec << " (" << ec.message() << ')';
            Error error{{}, reqType, rid, ec, {{"message", oss.str()}}};
            s.sendError(std::move(error), logOnly);
            return false;
        }

        if (!auth.allowed())
        {
            s.sendError(reqType, rid, SessionErrc::notAuthorized, logOnly);
            return false;
        }

        return true;
    }

    template <typename TData>
    static void setDisclosed(TData& data, DisclosureRule realmRule,
                             const Authorization& authentication)
    {
        auto rule = authentication.disclosure();
        rule = (rule == DisclosureRule::preset) ? realmRule : rule;
        setDisclosed(data, rule);
    }

    template <typename TData>
    static void setDisclosed(TData& data, DisclosureRule rule)
    {
        bool disclosed = data.discloseMe();
        if (rule == DisclosureRule::off)
            disclosed = false;
        if (rule == DisclosureRule::on)
            disclosed = true;
        data.setDisclosed({}, disclosed);
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

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand(), F{shared_from_this(), std::forward<Ts>(args)...});
    }

    template <typename TDirectOp, typename TAuthorizedOp, typename D>
    void dispatchAuthorized(AuthorizationAction action,
                            RealmSession::Ptr originator, D&& data)
    {
        if (config_.authorizer())
        {
            dispatchDynamicallyAuthorized<TAuthorizedOp>(
                action, std::move(originator), std::move(data));
        }
        else
        {
            safelyDispatch<TDirectOp>(originator, std::forward<D>(data));
        }
    }

    template <typename TOperation, typename D>
    void dispatchDynamicallyAuthorized(AuthorizationAction action,
                                       RealmSession::Ptr originator, D&& data)
    {
        struct Dispatched
        {
            Ptr self;
            AuthorizationRequest request;
            void operator()() {self->config_.authorizer()(std::move(request));}
        };

        auto self = shared_from_this();
        const auto& authorizer = config_.authorizer();
        AuthorizationHandler<TOperation, D> handler{self, originator,
                                                    TOperation{}};

        AuthorizationRequest req{{}, action, std::move(data),
                                 originator->sharedAuthInfo(),
                                 std::move(handler)};

        auto exec =
            boost::asio::get_associated_executor(authorizer, strand_);

        Dispatched dispatched{std::move(self), std::move(req)};

        dispatchAny(
            strand_,
            boost::asio::bind_executor(exec, std::move(dispatched)));
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
            }
        };

        safelyDispatch<Dispatched>(sid);
    }

    void subscribe(RealmSession::Ptr s, Topic&& topic)
    {
        struct Direct
        {
            Ptr self;
            RealmSession::Ptr s;
            Topic t;

            void operator()()
            {
                auto rid = t.requestId({});
                auto result = self->broker_.subscribe(s, std::move(t));
                if (result)
                    s->sendSubscribed(rid, *result);
                else
                    s->sendError(WampMsgType::subscribe, rid, result);
            }
        };

        struct Authorized
        {
            void operator()(Ptr self, RealmSession::Ptr s,
                            const Authorization& a, Topic&& t)
            {
                auto rid = t.requestId({});
                if (!checkAuthorization(a, *s, WampMsgType::subscribe, rid))
                    return;
                auto result = self->broker_.subscribe(s, std::move(t));
                if (result)
                    s->sendSubscribed(rid, *result);
                else
                    s->sendError(WampMsgType::subscribe, rid, result);
            }
        };

        dispatchAuthorized<Direct, Authorized>(
            AuthorizationAction::subscribe, std::move(s), std::move(topic));
    }

    void unsubscribe(RealmSession::Ptr s, SubscriptionId subId, RequestId rid)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr s;
            SubscriptionId subId;
            RequestId rid;

            void operator()()
            {
                auto result = self->broker_.unsubscribe(s, subId);
                if (result)
                    s->sendUnsubscribed(rid, *result);
                else
                    s->sendError(WampMsgType::unsubscribe, rid, result);
            }
        };

        safelyDispatch<Dispatched>(std::move(s), subId, rid);
    }

    void publish(RealmSession::Ptr s, Pub&& pub)
    {
        struct Direct
        {
            Ptr self;
            RealmSession::Ptr s;
            Pub p;

            void operator()()
            {
                auto rid = p.requestId({});
                bool ack = p.optionOr<bool>("acknowledge", false);
                setDisclosed(p, self->config_.publisherDisclosure());
                auto result = self->broker_.publish(s, std::move(p));
                if (ack)
                {
                    if (result)
                        s->sendPublished(rid, *result);
                    else
                        s->sendError(WampMsgType::publish, rid, result);
                }
            }
        };

        struct Authorized
        {
            void operator()(Ptr self, RealmSession::Ptr s,
                            const Authorization& a, Pub&& p)
            {
                auto rid = p.requestId({});
                bool ack = p.optionOr<bool>("acknowledge", false);
                if (!checkAuthorization(a, *s, WampMsgType::publish, rid, !ack))
                    return;
                if (a.hasTrustLevel())
                    p.setTrustLevel({}, a.trustLevel());
                setDisclosed(p, self->config_.publisherDisclosure(), a);
                auto result = self->broker_.publish(s, std::move(p));
                if (ack)
                {
                    if (result)
                        s->sendPublished(rid, *result);
                    else
                        s->sendError(WampMsgType::publish, rid, result);
                }
            }
        };

        dispatchAuthorized<Direct, Authorized>(
            AuthorizationAction::publish, std::move(s), std::move(pub));
    }

    void enroll(RealmSession::Ptr s, Procedure&& proc)
    {
        struct Direct
        {
            Ptr self;
            RealmSession::Ptr s;
            Procedure p;

            void operator()()
            {
                auto rid = p.requestId({});
                auto result = self->dealer_.enroll(s, std::move(p));
                if (result)
                    s->sendRegistered(rid, *result);
                else
                    s->sendError(WampMsgType::enroll, rid, result);
            }
        };

        struct Authorized
        {
            void operator()(Ptr self, RealmSession::Ptr s,
                            const Authorization& a, Procedure&& p)
            {
                auto rid = p.requestId({});
                if (!checkAuthorization(a, *s, WampMsgType::enroll, rid))
                    return;
                auto result = self->dealer_.enroll(s, std::move(p));
                if (result)
                    s->sendRegistered(rid, *result);
                else
                    s->sendError(WampMsgType::enroll, rid, result);
            }
        };

        dispatchAuthorized<Direct, Authorized>(
            AuthorizationAction::enroll, std::move(s), std::move(proc));
    }

    void unregister(RealmSession::Ptr s, RegistrationId regId, RequestId reqId)
    {
        struct Dispatched
        {
            Ptr self;
            RealmSession::Ptr s;
            RegistrationId regId;
            RequestId reqId;

            void operator()()
            {
                auto result = self->dealer_.unregister(s, regId);
                if (result)
                    s->sendUnregistered(reqId, *result);
                else
                    s->sendError(WampMsgType::unregister, reqId, result);
            }
        };

        safelyDispatch<Dispatched>(std::move(s), regId, reqId);
    }

    void call(RealmSession::Ptr s, Rpc&& rpc)
    {
        struct Direct
        {
            Ptr self;
            RealmSession::Ptr s;
            Rpc r;

            void operator()()
            {
                auto rid = r.requestId({});
                setDisclosed(r, self->config_.callerDisclosure());
                auto result = self->dealer_.call(s, std::move(r));
                if (!result)
                    s->sendError(WampMsgType::call, rid, result);
            }
        };

        struct Authorized
        {
            void operator()(Ptr self, RealmSession::Ptr s,
                            const Authorization& a, Rpc&& r)
            {
                auto rid = r.requestId({});
                if (!checkAuthorization(a, *s, WampMsgType::call, rid))
                    return;
                if (a.hasTrustLevel())
                    r.setTrustLevel({}, a.trustLevel());
                setDisclosed(r, self->config_.callerDisclosure(), a);
                auto result = self->dealer_.call(s, std::move(r));
                if (!result)
                    s->sendError(WampMsgType::call, rid, result);
            }
        };

        dispatchAuthorized<Direct, Authorized>(
            AuthorizationAction::enroll, std::move(s), std::move(rpc));
    }

    void cancelCall(RealmSession::Ptr s, CallCancellation&& c)
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
                    s->sendError(WampMsgType::call, rid, result);
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(c));
    }

    void yieldResult(RealmSession::Ptr s, Result&& r)
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

    void yieldError(RealmSession::Ptr s, Error&& e)
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

    friend class RealmContext;
};


//******************************************************************************
// RealmContext
//******************************************************************************

inline RealmContext::RealmContext(std::shared_ptr<RouterRealm> r)
    : realm_(std::move(r))
{}

inline bool RealmContext::expired() const
{
    return realm_.expired();
}

RealmContext::operator bool() const {return !realm_.expired();}

inline IoStrand RealmContext::strand() const
{
    auto r = realm_.lock();
    if (r)
        return r->strand();
    return {};
}

inline RouterLogger::Ptr RealmContext::logger() const
{
    auto r = realm_.lock();
    if (r)
        return r->logger();
    return {};
}

inline void RealmContext::reset() {realm_.reset();}

inline void RealmContext::join(RealmSessionPtr s)
{
    auto r = realm_.lock();
    if (r)
        r->join(std::move(s));
    realm_.reset();
}

inline void RealmContext::leave(SessionId sid)
{
    auto r = realm_.lock();
    if (r)
        r->leave(sid);
    realm_.reset();
}

inline void RealmContext::subscribe(RealmSessionPtr s, Topic t)
{
    auto r = realm_.lock();
    if (r)
        r->subscribe(std::move(s), std::move(t));

}

inline void RealmContext::unsubscribe(RealmSessionPtr s, SubscriptionId subId,
                                      RequestId rid)
{
    auto r = realm_.lock();
    if (r)
        r->unsubscribe(std::move(s), subId, rid);
}

inline void RealmContext::publish(RealmSessionPtr s, Pub pub)
{
    auto r = realm_.lock();
    if (r)
        r->publish(std::move(s), std::move(pub));
}

inline void RealmContext::enroll(RealmSessionPtr s, Procedure proc)
{
    auto r = realm_.lock();
    if (r)
        r->enroll(std::move(s), std::move(proc));
}

inline void RealmContext::unregister(RealmSessionPtr s, RegistrationId regId,
                                     RequestId reqId)
{
    auto r = realm_.lock();
    if (r)
        r->unregister(std::move(s), regId, reqId);
}

inline void RealmContext::call(RealmSessionPtr s, Rpc rpc)
{
    auto r = realm_.lock();
    if (r)
        r->call(std::move(s), std::move(rpc));
}

inline void RealmContext::cancelCall(RealmSessionPtr s, CallCancellation c)
{
    auto r = realm_.lock();
    if (r)
        r->cancelCall(std::move(s), std::move(c));
}

inline void RealmContext::yieldResult(RealmSessionPtr s, Result result)
{
    auto r = realm_.lock();
    if (r)
        r->yieldResult(std::move(s), std::move(result));
}

inline void RealmContext::yieldError(RealmSessionPtr s, Error e)
{
    auto r = realm_.lock();
    if (r)
        r->yieldError(std::move(s), std::move(e));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
