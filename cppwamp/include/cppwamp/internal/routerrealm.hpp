/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include "../routerconfig.hpp"
#include "../uri.hpp"
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

        SubscriptionId subId = nullId();

        auto iter = subsByTopic_.find(topic);
        if (iter == subsByTopic_.end())
        {
            subId = nextSubId();
            SubInfo info{std::move(s), subId};
            iter = subsByTopic_.emplace(std::move(topic),
                                        std::move(info)).first;
            topicsBySubId_.emplace(subId, iter);
        }
        else
        {
            auto& info = iter->second;
            subId = info.subId;
            info.addSession(std::move(s));
        }

        return subId;
    }

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sid)
    {
        auto found = topicsBySubId_.find(subId);
        if (found == topicsBySubId_.end())
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        auto subsByTopicIter = found->second;
        auto& info = subsByTopicIter->second;
        bool erased = info.removeSession(sid);
        if (info.sessions.empty())
        {
            subsByTopic_.erase(subsByTopicIter);
            topicsBySubId_.erase(found);
        }
        if (!erased)
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        return true;
    }

    ErrorOr<PublicationId> publish(const Pub& pub, SessionId sid)
    {
        // TODO: publish and event options
        auto pubId = pubIdGenerator_();
        publishExactMatches(pub, sid, pubId);
        publishPrefixMatches(pub, sid, pubId);
        publishWildcardMatches(pub, sid, pubId);
        return pubId;
    }

private:
    using Policy = Topic::MatchPolicy;

    struct UriAndPolicy
    {
        String uri;
        Policy policy;

        explicit UriAndPolicy(String uri, Policy p = Policy::unknown)
            : uri(std::move(uri)),
              policy(p)
        {}

        explicit UriAndPolicy(Topic&& t)
            : UriAndPolicy(std::move(t).uri({}), t.matchPolicy())
        {}

        bool uriIsValid() const
        {
            // TODO:
            return true;
        }

        bool policyIsKnown() const {return policy != Policy::unknown;}

        bool operator<(const UriAndPolicy& rhs) const
        {
            return std::tie(policy, uri) < std::tie(rhs.policy, rhs.uri);
        }
    };

    struct SubInfo
    {
        SubInfo(RouterSession::Ptr s, SubscriptionId subId)
            : sessions({{s->wampId(), s}}),
              subId(subId)
        {}

        SubInfo(RouterSession::Ptr s, SubscriptionId subId,
                const String& patternUri)
            : sessions({{s->wampId(), s}}),
              pattern(tokenizeUri(patternUri)),
              subId(subId)
        {}

        void addSession(RouterSession::Ptr s)
        {
            sessions.emplace(s->wampId(), s);
        }

        bool removeSession(SessionId sid)
        {
            return sessions.erase(sid) != 0;
        }

        std::map<SessionId, RouterSession::WeakPtr> sessions;
        SplitUri pattern;
        SubscriptionId subId;
    };

    using SubsByTopic = std::map<UriAndPolicy, SubInfo>;

    static Event eventFromPub(const Pub& pub, SubscriptionId subId,
                              PublicationId pubId, Object opts)
    {
        Event ev{subId, pubId, std::move(opts)};
        if (!pub.args().empty() || !pub.kwargs().empty())
            ev.withArgList(pub.args());
        if (!pub.kwargs().empty())
            ev.withKwargs(pub.kwargs());
        return ev;
    }

    static bool startsWith(const String& s, const String& prefix)
    {
        // https://stackoverflow.com/a/40441240/245265
        return s.rfind(prefix, 0) == 0;
    }

    SubscriptionId nextSubId()
    {
        auto s = nextSubscriptionId_;
        while ((s == nullId()) || (topicsBySubId_.count(s) == 1))
            ++s;
        nextSubscriptionId_ = s + 1;
        return s;
    }

    void publishExactMatches(const Pub& pub, SessionId sid, PublicationId pubId)
    {
        UriAndPolicy key{pub.topic(),  Policy::exact};
        auto found = subsByTopic_.find(key);
        if (found != subsByTopic_.end())
            publishMatches(pub, sid, pubId, found->second);
    }

    void publishPrefixMatches(const Pub& pub, SessionId sid,
                              PublicationId pubId)
    {
        // TODO: Use trie or similar data structure

        const auto& pubUri = pub.topic();
        UriAndPolicy key{pubUri, Policy::prefix};
        auto begin = subsByTopic_.begin();
        auto iter = subsByTopic_.upper_bound(key);
        while (iter != begin)
        {
            --iter;
            const auto& topic = iter->first;
            if (topic.policy != Policy::prefix ||
                !startsWith(pubUri, topic.uri))
            {
                break;
            }
            publishMatches(pub, sid, pubId, iter->second);
        }
    }

    void publishWildcardMatches(const Pub& pub, SessionId sid,
                                PublicationId pubId)
    {
        // TODO: Figure out how to do better than O(N * M)

        struct Compare
        {
            bool operator()(const SubsByTopic::value_type& kv, Policy p) const
            {
                return kv.first.policy == p;
            };

            bool operator()(Policy p, const SubsByTopic::value_type& kv) const
            {
                return kv.first.policy == p;
            };
        };

        auto splitPubUri = tokenizeUri(pub.topic());

        auto range = std::equal_range(subsByTopic_.begin(), subsByTopic_.end(),
                                      Policy::wildcard, Compare{});

        for (auto iter = range.first; iter != range.second; ++iter)
        {
            const auto& info = iter->second;
            if (uriMatchesWildcardPattern(splitPubUri, info.pattern))
                publishMatches(pub, sid, pubId, info);
        }
    }

    void publishMatches(const Pub& pub, SessionId sid, PublicationId pubId,
                         const SubInfo& info)
    {
        auto ev = eventFromPub(pub, info.subId, pubId, {});
        for (auto& kv : info.sessions)
        {
            auto session = kv.second.lock();
            if (session)
                session->sendEvent(Event{ev});
        }
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
