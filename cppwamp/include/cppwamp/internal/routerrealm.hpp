/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <map>
#include <memory>
#include <thread>
#include <string>
#include <utility>
#include "../routerconfig.hpp"
#include "localsessionimpl.hpp"
#include "routercontext.hpp"
#include "routerserver.hpp"

namespace wamp
{


namespace internal
{

class LocalSessionImpl;

//------------------------------------------------------------------------------
class RouterRealm : public std::enable_shared_from_this<RouterRealm>
{
public:
    using Ptr = std::shared_ptr<RouterRealm>;
    using Executor = AnyIoExecutor;

    static Ptr create(Executor e, RealmConfig c, RouterContext r)
    {
        return Ptr(new RouterRealm(std::move(e), std::move(c), std::move(r)));
    }

    const IoStrand& strand() const {return strand_;}

    const std::string& uri() const {return config_.uri();}

    void join(RouterSession::Ptr session)
    {
        auto id = session->wampId();
        sessions_.emplace(id, std::move(session));
    }

    void close(bool terminate, Reason r)
    {
        std::string msg = terminate ? "Shutting down realm with reason "
                                    : "Terminating realm with reason ";
        msg += r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        log({LogLevel::info, std::move(msg)});

        for (auto& kv: sessions_)
            kv.second->close(terminate, r);
        sessions_.clear();
    }

private:
    RouterRealm(Executor&& e, RealmConfig&& c, RouterContext&& r)
        : strand_(boost::asio::make_strand(e)),
          router_(std::move(r)),
          config_(std::move(c)),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          logger_(router_.logger())
    {}

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    RouterLogger::Ptr logger() const {return logger_;}

    template <typename TSessionLike>
    void safeLeave(std::shared_ptr<TSessionLike> s)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<TSessionLike> s;
            void operator()() {self->leave(std::move(s));}
        };

        dispatch(Dispatched{shared_from_this(), std::move(s)});
    }

    void leave(RouterSession::Ptr session)
    {
        sessions_.erase(session->wampId());
    }

    void safeOnMessage(std::shared_ptr<ServerSession> s, WampMessage m)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<ServerSession> s;
            WampMessage m;

            void operator()()
            {
                self->onMessage(std::move(s), std::move(m));
            }
        };

        dispatch(Dispatched{shared_from_this(), std::move(s), std::move(m)});
    }

    void onMessage(std::shared_ptr<ServerSession> s, WampMessage m)
    {
        using M = WampMsgType;
        switch (m.type())
        {
        case M::error:       return onError(std::move(s), m);
        case M::publish:     return onPublish(std::move(s), m);
        case M::subscribe:   return onSubscribe(std::move(s), m);
        case M::unsubscribe: return onUnsubscribe(std::move(s), m);
        case M::call:        return onCall(std::move(s), m);
        case M::cancel:      return onCancel(std::move(s), m);
        case M::enroll:      return onRegister(std::move(s), m);
        case M::unregister:  return onUnregister(std::move(s), m);
        case M::yield:       return onYield(std::move(s), m);
        default:             assert(false && "Unexpected message type"); break;
        }
    }

    void onError(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<ErrorMessage>(m);
        s->logAccess({"client-error", {}, msg.options(), msg.reasonUri(),
                      false});
    }

    void onPublish(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<PublishMessage>(m);
        s->logAccess({"client-publish", msg.topicUri(), msg.options()});
    }

    void onSubscribe(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<SubscribeMessage>(m);
        s->logAccess({"client-subscribe", msg.topicUri(), msg.options()});
    }

    void onUnsubscribe(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<UnsubscribeMessage>(m);
        s->logAccess({"client-unsubscribe"});
    }

    void onCall(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<CallMessage>(m);
        s->logAccess({"client-call", msg.procedureUri(), msg.options()});
    }

    void onCancel(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<CancelMessage>(m);
        s->logAccess({"client-cancel", {}, msg.options()});
    }

    void onRegister(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<RegisterMessage>(m);
        s->logAccess({"client-register", msg.procedureUri(), msg.options()});
    }

    void onUnregister(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<UnregisterMessage>(m);
        s->logAccess({"client-unregister"});
    }

    void onYield(std::shared_ptr<ServerSession> s, WampMessage& m)
    {
        auto& msg = messageCast<YieldMessage>(m);
        s->logAccess({"client-yield", {}, msg.options()});
    }

    IoStrand strand_;
    RouterContext router_;
    std::map<SessionId, RouterSession::Ptr> sessions_;
    RealmConfig config_;
    std::string logSuffix_;
    RouterLogger::Ptr logger_;

    // TODO: Consider common interface for local and server sessions

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

inline void RealmContext::onMessage(std::shared_ptr<ServerSession> s,
                                    WampMessage m)
{
    auto r = realm_.lock();
    if (r)
        r->safeOnMessage(std::move(s), std::move(m));
}

inline void RealmContext::leave(std::shared_ptr<LocalSessionImpl> s)
{
    auto r = realm_.lock();
    if (r)
        r->safeLeave(std::move(s));
    realm_.reset();
}

inline void RealmContext::leave(std::shared_ptr<ServerSession> s)
{
    auto r = realm_.lock();
    if (r)
        r->safeLeave(std::move(s));
    realm_.reset();
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
