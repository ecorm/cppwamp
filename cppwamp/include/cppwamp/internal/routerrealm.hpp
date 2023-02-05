/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include "../routerconfig.hpp"
#include "idgen.hpp"
#include "realmbroker.hpp"
#include "realmdealer.hpp"
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

    static Ptr create(Executor e, RealmConfig c, RouterContext r)
    {
        return Ptr(new RouterRealm(std::move(e), std::move(c), std::move(r)));
    }

    const IoStrand& strand() const {return strand_;}

    const std::string& uri() const {return config_.uri();}

    void join(RouterSession::Ptr session)
    {
        auto reservedId = router_.reserveSessionId();
        auto id = reservedId.get();
        session->setWampId({}, std::move(reservedId));
        MutexGuard lock(mutex_);
        sessions_.emplace(id, std::move(session));
    }

    void close(bool terminate, Reason r)
    {
        MutexGuard lock(mutex_);
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
    using MutexGuard = std::lock_guard<std::mutex>;

    RouterRealm(Executor&& e, RealmConfig&& c, RouterContext&& r)
        : strand_(boost::asio::make_strand(e)),
          router_(std::move(r)),
          dealer_(strand_),
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

    void leave(SessionId sid)
    {
        MutexGuard lock(mutex_);
        sessions_.erase(sid);
    }

    ErrorOr<SubscriptionId> subscribe(RouterSession::Ptr s, Topic&& t)
    {
        MutexGuard lock(mutex_);
        return broker_.subscribe(std::move(s), std::move(t));
    }

    ErrorOrDone unsubscribe(RouterSession::Ptr s, SubscriptionId subId)
    {
        MutexGuard lock(mutex_);
        return broker_.unsubscribe(std::move(s), subId);
    }

    ErrorOr<PublicationId> publish(RouterSession::Ptr s, Pub&& pub)
    {
        MutexGuard lock(mutex_);
        return broker_.publish(std::move(s), std::move(pub));
    }

    ErrorOr<RegistrationId> enroll(RouterSession::Ptr s, Procedure&& proc)
    {
        MutexGuard lock(mutex_);
        return dealer_.enroll(std::move(s), std::move(proc));
    }

    ErrorOrDone unregister(RouterSession::Ptr s, RegistrationId rid)
    {
        MutexGuard lock(mutex_);
        return dealer_.unregister(std::move(s), rid);
    }

    ErrorOrDone call(RouterSession::Ptr s, Rpc&& rpc)
    {
        MutexGuard lock(mutex_);
        return dealer_.call(std::move(s), std::move(rpc));
    }

    ErrorOrDone cancelCall(RouterSession::Ptr s, CallCancellation&& c)
    {
        MutexGuard lock(mutex_);
        return dealer_.cancelCall(std::move(s), std::move(c));
    }

    void yieldResult(RouterSession::Ptr s, Result&& r)
    {
        MutexGuard lock(mutex_);
        dealer_.yieldResult(std::move(s), std::move(r));
    }

    void yieldError(RouterSession::Ptr s, Error&& e)
    {
        MutexGuard lock(mutex_);
        dealer_.yieldError(std::move(s), std::move(e));
    }

    IoStrand strand_;
    RouterContext router_;
    std::map<SessionId, RouterSession::Ptr> sessions_;
    RealmBroker broker_;
    RealmDealer dealer_;
    RealmConfig config_;
    std::mutex mutex_;
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

inline ErrorOr<SubscriptionId> RealmContext::subscribe(RouterSessionPtr s,
                                                       Topic t)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->subscribe(std::move(s), std::move(t));

}

inline ErrorOrDone RealmContext::unsubscribe(RouterSessionPtr s,
                                             SubscriptionId subId)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->unsubscribe(std::move(s), subId);
}

inline ErrorOr<PublicationId> RealmContext::publish(RouterSessionPtr s,
                                                    Pub pub)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->publish(std::move(s), std::move(pub));
}

inline ErrorOr<RegistrationId> RealmContext::enroll(RouterSessionPtr s,
                                                    Procedure proc)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->enroll(std::move(s), std::move(proc));
}

inline ErrorOrDone RealmContext::unregister(RouterSessionPtr s,
                                            RegistrationId rid)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->unregister(std::move(s), rid);
}

inline ErrorOrDone RealmContext::call(RouterSessionPtr s, Rpc rpc)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->call(std::move(s), std::move(rpc));
}

inline ErrorOrDone RealmContext::cancelCall(RouterSessionPtr s,
                                            CallCancellation c)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    return r->cancelCall(std::move(s), std::move(c));
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
