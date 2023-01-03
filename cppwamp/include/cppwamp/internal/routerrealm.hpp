/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include "../routerconfig.hpp"
#include "idgen.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

namespace wamp
{


namespace internal
{

//------------------------------------------------------------------------------
class RealmBroker
{
public:
    ErrorOr<SubscriptionId> subscribe(Topic&& t, RouterSession::Ptr s)
    {
        UriAndPolicy topic{std::move(t)};
        if (!topic.uriIsValid())
            return makeUnexpectedError(SessionErrc::invalidUri);
        if (!topic.policyIsKnown())
            return makeUnexpectedError(SessionErrc::optionNotAllowed);

        auto sessionId = s->wampId();
        SubscriptionId subId = nullId();

        auto iter = subsByTopic_.find(topic);
        if (iter == subsByTopic_.end())
        {
            subId = nextSubId();
            SubInfo info{{{sessionId, std::move(s)}}, subId};
            iter = subsByTopic_.emplace(std::move(topic),
                                        std::move(info)).first;
            topicsBySubId_.emplace(subId, iter);
        }
        else
        {
            auto& info = iter->second;
            subId = info.subId;
            info.sessions.emplace(sessionId, std::move(s));
        }

        return subId;
    }

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sid)
    {
        // TODO
        return true;
    }

    ErrorOr<PublicationId> publish(Pub&& p, SessionId sid)
    {
        // TODO
        return true;
    }

private:
    using Policy = Topic::MatchPolicy;

    struct UriAndPolicy
    {
        String uri;
        Topic::MatchPolicy policy;

        explicit UriAndPolicy(Topic&& t)
            : uri(std::move(t).uri({})),
              policy(t.matchPolicy())
        {}

        bool uriIsValid() const
        {
            // TODO:
            return true;
        }

        bool policyIsKnown() const {return policy != Policy::unknown;}

        bool operator<(const UriAndPolicy& rhs) const
        {
            return std::tie(uri, policy) < std::tie(rhs.uri, rhs.policy);
        }

        bool operator<(const String& topicUri) const
        {
            return uri < topicUri;
        }
    };

    struct SubInfo
    {
        std::map<SessionId, RouterSession::WeakPtr> sessions;
        SubscriptionId subId;
    };

    using SubsByTopic = std::map<UriAndPolicy, SubInfo>;

    SubscriptionId nextSubId()
    {
        auto s = nextSubscriptionId_;
        while ((s == nullId()) || (topicsBySubId_.count(s) == 1))
            ++s;
        nextSubscriptionId_ = s + 1;
        return s;
    }

    SubsByTopic subsByTopic_;
    std::map<SubscriptionId, SubsByTopic::iterator> topicsBySubId_;
    EphemeralId nextSubscriptionId_ = nullId();
    RandomIdGenerator pubIdGenerator_;
};

//------------------------------------------------------------------------------
class RealmDealer
{
public:
    ErrorOr<RegistrationId> enroll(Procedure&& p, RouterSession::Ptr s)
    {
        // TODO
        return 0;
    }

    ErrorOrDone unregister(RegistrationId rid, SessionId sid)
    {
        // TODO
        return true;
    }

    ErrorOrDone call(Rpc&& p, SessionId sid)
    {
        // TODO
        return true;
    }

    bool cancel(RequestId rid, SessionId sid)
    {
        // TODO
        return true;
    }

    void yieldResult(Result&& r, SessionId sid)
    {
        // TODO
    }

    void yieldError(Error&& e, SessionId sid)
    {
        // TODO
    }
};

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

    ErrorOr<SubscriptionId> subscribe(Topic&& t, RouterSession::Ptr s)
    {
        MutexGuard lock(mutex_);
        return broker_.subscribe(std::move(t), std::move(s));
    }

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sessionId)
    {
        MutexGuard lock(mutex_);
        return broker_.unsubscribe(subId, sessionId);
    }

    ErrorOr<PublicationId> publish(Pub&& pub, SessionId sid)
    {
        MutexGuard lock(mutex_);
        return broker_.publish(std::move(pub), sid);
    }

    ErrorOr<RegistrationId> enroll(Procedure&& proc, RouterSession::Ptr s)
    {
        MutexGuard lock(mutex_);
        return dealer_.enroll(std::move(proc), std::move(s));
    }

    ErrorOrDone unregister(RegistrationId rid, SessionId sid)
    {
        MutexGuard lock(mutex_);
        return dealer_.unregister(rid, sid);
    }

    ErrorOrDone call(Rpc&& rpc, SessionId sid)
    {
        MutexGuard lock(mutex_);
        return dealer_.call(std::move(rpc), sid);
    }

    bool cancelCall(RequestId rid, SessionId sid)
    {
        MutexGuard lock(mutex_);
        return dealer_.cancel(rid, sid);
    }

    void yieldResult(Result&& r, SessionId sid)
    {
        MutexGuard lock(mutex_);
        dealer_.yieldResult(std::move(r), sid);
    }

    void yieldError(Error&& e, SessionId sid)
    {
        MutexGuard lock(mutex_);
        dealer_.yieldError(std::move(e), sid);
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

inline ErrorOr<SubscriptionId> RealmContext::subscribe(Topic t,
                                                       RouterSessionPtr s)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->subscribe(std::move(t), std::move(s));

}

inline ErrorOrDone RealmContext::unsubscribe(SubscriptionId subId,
                                             SessionId sessionId)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->unsubscribe(subId, sessionId);
}

inline ErrorOr<PublicationId> RealmContext::publish(Pub pub, SessionId sid)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->publish(std::move(pub), sid);
}

inline ErrorOr<RegistrationId> RealmContext::enroll(Procedure proc,
                                                    RouterSessionPtr s)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->enroll(std::move(proc), std::move(s));
}

inline ErrorOrDone RealmContext::unregister(RegistrationId rid, SessionId sid)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->unregister(rid, sid);
}

inline ErrorOrDone RealmContext::call(Rpc rpc, SessionId sid)
{
    auto r = realm_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->call(std::move(rpc), sid);
}

inline bool RealmContext::cancelCall(RequestId rid, SessionId sid)
{
    auto r = realm_.lock();
    if (!r)
        return false;
    return r->cancelCall(rid, sid);
}

inline void RealmContext::yieldResult(Result result, SessionId sid)
{
    auto r = realm_.lock();
    if (r)
        r->yieldResult(std::move(result), sid);
}

inline void RealmContext::yieldError(Error e, SessionId sid)
{
    auto r = realm_.lock();
    if (r)
        r->yieldError(std::move(e), sid);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
