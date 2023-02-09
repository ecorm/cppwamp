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
    static bool checkAuthorization(const Authorization& auth, RouterSession& s,
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

    RouterRealm(Executor&& e, RealmConfig&& c, const RouterConfig& rcfg,
                RouterContext&& rctx)
        : strand_(boost::asio::make_strand(e)),
          config_(std::move(c)),
          router_(std::move(rctx)),
          broker_(rcfg.publicationRNG()),
          dealer_(strand_),
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

    template <typename TOperation, typename TPeerData>
    void dispatchAuthorized(AuthorizationAction a, RouterSession::Ptr s,
                            TPeerData&& d)
    {
        struct Dispatched
        {
            Ptr self;
            RealmConfig::Authorizer authorizer;
            AuthorizationRequest request;

            void operator()()
            {
                authorizer(std::move(request));
            }
        };

        if (config_.authorizer())
        {
            const auto& authorizer = config_.authorizer();
            AuthorizationRequest req{{}, a, std::forward<TPeerData>(d),
                                     s->sharedAuthInfo(), strand_,
                                     TOperation{shared_from_this(), s}};
            auto exec = boost::asio::get_associated_executor(authorizer,
                                                             strand_);
            Dispatched dispatched{shared_from_this(), config_.authorizer(),
                                  std::move(req)};
            dispatchAny(strand_,
                        boost::asio::bind_executor(exec,
                                                   std::move(dispatched)));
        }
        else
        {
            dispatchAny(strand_, TOperation{shared_from_this(), s},
                        std::forward<TPeerData>(d));
        }
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
                self->sessions_.erase(sid);
            }
        };

        safelyDispatch<Dispatched>(sid);
    }

    void subscribe(RouterSession::Ptr s, Topic&& topic)
    {
        struct Op
        {
            Ptr self;
            RouterSession::Ptr s;

            void operator()(Authorization a)
            {
                auto& topic = a.dataAs<Topic>({});
                auto reqId = topic.requestId({});
                if (!checkAuthorization(a, *s, WampMsgType::subscribe, reqId))
                    return;
                (*this)(std::move(topic));
            }

            void operator()(Topic topic)
            {
                auto reqId = topic.requestId({});
                auto result = self->broker_.subscribe(s, std::move(topic));
                if (result)
                    s->sendSubscribed(reqId, *result);
                else
                    s->sendError(WampMsgType::subscribe, reqId, result);
            }
        };

        dispatchAuthorized<Op>(AuthorizationAction::subscribe, std::move(s),
                               std::move(topic));
    }

    void unsubscribe(RouterSession::Ptr s, SubscriptionId subId, RequestId rid)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
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

    void publish(RouterSession::Ptr s, Pub&& pub)
    {
        struct Op
        {
            Ptr self;
            RouterSession::Ptr s;

            void operator()(Authorization a)
            {
                auto& pub = a.dataAs<Pub>({});
                auto rid = pub.requestId({});
                bool ack = pub.optionOr<bool>("acknowledge", false);
                if (!checkAuthorization(a, *s, WampMsgType::publish, rid, !ack))
                    return;
                publish(std::move(pub), rid, ack);
            }

            void operator()(Pub pub)
            {
                auto rid = pub.requestId({});
                bool ack = pub.optionOr<bool>("acknowledge", false);
                publish(std::move(pub), rid, ack);
            }

            void publish(Pub&& pub, RequestId rid, bool ack)
            {
                auto result = self->broker_.publish(s, std::move(pub));
                if (ack)
                {
                    if (result)
                        s->sendPublished(rid, *result);
                    else
                        s->sendError(WampMsgType::publish, rid, result);
                }
            }
        };

        dispatchAuthorized<Op>(AuthorizationAction::publish, std::move(s),
                               std::move(pub));
    }

    void enroll(RouterSession::Ptr s, Procedure&& proc)
    {
        struct Op
        {
            Ptr self;
            RouterSession::Ptr s;

            void operator()(Authorization a)
            {
                auto& proc = a.dataAs<Procedure>({});
                auto rid = proc.requestId({});
                if (!checkAuthorization(a, *s, WampMsgType::enroll, rid))
                    return;
                (*this)(std::move(proc));
            }

            void operator()(Procedure proc)
            {
                auto rid = proc.requestId({});
                auto result = self->dealer_.enroll(s, std::move(proc));
                if (result)
                    s->sendRegistered(rid, *result);
                else
                    s->sendError(WampMsgType::enroll, rid, result);
            }
        };

        dispatchAuthorized<Op>(AuthorizationAction::enroll, std::move(s),
                               std::move(proc));
    }

    void unregister(RouterSession::Ptr s, RegistrationId regId, RequestId reqId)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
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

    void call(RouterSession::Ptr s, Rpc&& rpc)
    {
        struct Op
        {
            Ptr self;
            RouterSession::Ptr s;

            void operator()(Authorization a)
            {
                auto& rpc = a.dataAs<Rpc>({});
                auto rid = rpc.requestId({});
                if (!checkAuthorization(a, *s, WampMsgType::call, rid))
                    return;
                (*this)(std::move(rpc));
            }

            void operator()(Rpc rpc)
            {
                auto rid = rpc.requestId({});
                auto result = self->dealer_.call(s, std::move(rpc));
                if (!result)
                    s->sendError(WampMsgType::call, rid, result);
            }
        };

        dispatchAuthorized<Op>(AuthorizationAction::enroll, std::move(s),
                               std::move(rpc));
    }

    void cancelCall(RouterSession::Ptr s, CallCancellation&& c)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
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

    void yieldResult(RouterSession::Ptr s, Result&& r)
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

        safelyDispatch<Dispatched>(std::move(s), std::move(r));
    }

    void yieldError(RouterSession::Ptr s, Error&& e)
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

        safelyDispatch<Dispatched>(std::move(s), std::move(e));
    }

    IoStrand strand_;
    RealmConfig config_;
    RouterContext router_;
    std::map<SessionId, RouterSession::Ptr> sessions_;
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

inline void RealmContext::join(RouterSessionPtr s)
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

inline void RealmContext::subscribe(RouterSessionPtr s, Topic t)
{
    auto r = realm_.lock();
    if (r)
        r->subscribe(std::move(s), std::move(t));

}

inline void RealmContext::unsubscribe(RouterSessionPtr s, SubscriptionId subId,
                                      RequestId rid)
{
    auto r = realm_.lock();
    if (r)
        r->unsubscribe(std::move(s), subId, rid);
}

inline void RealmContext::publish(RouterSessionPtr s, Pub pub)
{
    auto r = realm_.lock();
    if (r)
        r->publish(std::move(s), std::move(pub));
}

inline void RealmContext::enroll(RouterSessionPtr s, Procedure proc)
{
    auto r = realm_.lock();
    if (r)
        r->enroll(std::move(s), std::move(proc));
}

inline void RealmContext::unregister(RouterSessionPtr s, RegistrationId regId,
                                     RequestId reqId)
{
    auto r = realm_.lock();
    if (r)
        r->unregister(std::move(s), regId, reqId);
}

inline void RealmContext::call(RouterSessionPtr s, Rpc rpc)
{
    auto r = realm_.lock();
    if (r)
        r->call(std::move(s), std::move(rpc));
}

inline void RealmContext::cancelCall(RouterSessionPtr s, CallCancellation c)
{
    auto r = realm_.lock();
    if (r)
        r->cancelCall(std::move(s), std::move(c));
}

inline void RealmContext::yieldResult(RouterSessionPtr s, Result result)
{
    auto r = realm_.lock();
    if (r)
        r->yieldResult(std::move(s), std::move(result));
}

inline void RealmContext::yieldError(RouterSessionPtr s, Error e)
{
    auto r = realm_.lock();
    if (r)
        r->yieldError(std::move(s), std::move(e));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
