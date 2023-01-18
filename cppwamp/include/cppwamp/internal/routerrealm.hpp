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
#include "../trie.hpp"
#include "../wildcardtrie.hpp"
#include "../wildcarduri.hpp"
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
        auto topicError = topic.check();
        if (topicError)
            return makeUnexpected(topicError);
        SessionId sid = s->wampId();
        SubscriberInfo info{std::move(s)};

        switch (topic.policy)
        {
        case Policy::unknown:
            return makeUnexpectedError(SessionErrc::optionNotAllowed);

        case Policy::exact:
        {
            auto key = topic.uri;
            return doSubscribe(byExact_, std::move(key), std::move(topic),
                               std::move(info), sid);
        }

        case Policy::prefix:
        {
            auto key = topic.uri;
            return doSubscribe(byPrefix_, std::move(key), std::move(topic),
                               std::move(info), sid);
        }

        case Policy::wildcard:
        {
            return doSubscribe(byWildcard_, topic.uri, std::move(topic),
                               std::move(info), sid);
        }

        default:
            break;
        }

        assert(false && "Unexpected Topic::MatchPolicy enumerator");
        return 0;
    }

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sessionId)
    {
        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        SubscriptionRecord& rec = found->second;
        bool erased = rec.removeSubscriber(sessionId);

        if (rec.sessions.empty())
        {
            UriAndPolicy topic = std::move(rec.topic);
            subscriptions_.erase(found);
            switch (topic.policy)
            {
            case Policy::exact:
                byExact_.erase(topic.uri);
                break;

            case Policy::prefix:
                byPrefix_.erase(topic.uri);
                break;

            case Policy::wildcard:
                byWildcard_.erase(topic.uri);
                break;

            default:
                assert(false && "Unexpected Topic::MatchPolicy enumerator");
                break;
            }
        }

        if (!erased)
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        return erased;
    }

    ErrorOr<PublicationId> publish(const Pub& pub, SessionId publisherId)
    {
        // TODO: publish and event options
        auto publicationId = pubIdGenerator_();
        publishExactMatches(pub, publisherId, publicationId);
        publishPrefixMatches(pub, publisherId, publicationId);
        publishWildcardMatches(pub, publisherId, publicationId);
        return publicationId;
    }

private:
    using Policy = Topic::MatchPolicy;

    struct UriAndPolicy
    {
        String uri;
        Policy policy;

        UriAndPolicy() = default;

        explicit UriAndPolicy(String uri, Policy p = Policy::unknown)
            : uri(std::move(uri)),
              policy(p)
        {}

        explicit UriAndPolicy(Topic&& t)
            : UriAndPolicy(std::move(t).uri({}), t.matchPolicy())
        {}

        std::error_code check() const
        {
            // TODO: Check valid URI
            return {};
        }
    };

    struct SubscriberInfo
    {
        RouterSession::WeakPtr session;
    };

    struct SubscriptionRecord
    {
        SubscriptionRecord() = default;

        SubscriptionRecord(UriAndPolicy topic) : topic(std::move(topic)) {}

        void addSubscriber(SessionId sid, SubscriberInfo info)
        {
            sessions.emplace(sid, std::move(info));
        }

        bool removeSubscriber(SessionId sid)
        {
            return sessions.erase(sid) != 0;
        }

        std::map<SessionId, SubscriberInfo> sessions;
        UriAndPolicy topic;
    };

    // Need associative container with stable iterators
    // for use in by-pattern maps.
    using SubscriptionMap = std::map<SubscriptionId, SubscriptionRecord>;

    SubscriptionMap subscriptions_;
    BasicTrieMap<char, SubscriptionMap::iterator> byExact_;
    BasicTrieMap<char, SubscriptionMap::iterator> byPrefix_;
    TokenTrie<SplitUri, SubscriptionMap::iterator> byWildcard_;
    EphemeralId nextSubscriptionId_ = nullId();
    RandomIdGenerator pubIdGenerator_;


    static Event eventFromPub(const Pub& pub, PublicationId pubId,
                              SubscriptionId subId, Object opts)
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

    template <typename TTrie, typename TKey>
    ErrorOr<SubscriptionId> doSubscribe(
        TTrie& trie, TKey&& key, UriAndPolicy&& topic, SubscriberInfo&& info,
        SessionId sessionId)
    {
        SubscriptionId subId = nullId();
        auto found = trie.find(key);
        if (found == trie.end())
        {
            subId = nextSubId();
            auto uri = topic.uri;
            SubscriptionRecord rec{std::move(topic)};
            rec.addSubscriber(sessionId, std::move(info));
            auto emplaced = subscriptions_.emplace(subId, std::move(rec));
            assert(emplaced.second);
            trie.emplace(std::move(key), emplaced.first);
        }
        else
        {
            subId = (*found)->first;
            SubscriptionRecord& rec = (*found)->second;
            rec.addSubscriber(sessionId, std::move(info));
        }
        return subId;
    }

    SubscriptionId nextSubId()
    {
        auto s = nextSubscriptionId_;
        while ((s == nullId()) || (subscriptions_.count(s) == 1))
            ++s;
        nextSubscriptionId_ = s + 1;
        return s;
    }

    void publishExactMatches(const Pub& pub, SessionId publisherId,
                             PublicationId publicationId)
    {
        auto found = byExact_.find(pub.topic());
        if (found != byExact_.end())
        {
            SubscriptionId subId = (*found)->first;
            const SubscriptionRecord& rec = (*found)->second;
            publishMatches(pub, publisherId, publicationId, subId, rec);
        }
    }

    void publishPrefixMatches(const Pub& pub, SessionId publisherId,
                              PublicationId publicationId)
    {
        auto range = byPrefix_.equal_prefix_range(pub.topic());
        for (; range.first != range.second; ++range.first)
        {
            SubscriptionId subId = (*range.first)->first;
            const SubscriptionRecord& rec = (*range.first)->second;
            publishMatches(pub, publisherId, publicationId, subId, rec);
        }
    }

    void publishWildcardMatches(const Pub& pub, SessionId publisherId,
                                PublicationId publicationId)
    {
        auto range = byWildcard_.match_range(pub.topic());
        for (; range.first != range.second; ++range.first)
        {
            SubscriptionId subId = (*range.first)->first;
            const SubscriptionRecord& rec = (*range.first)->second;
            publishMatches(pub, publisherId, publicationId, subId, rec);
        }
    }

    void publishMatches(const Pub& pub, SessionId publisherId,
                        PublicationId publicationId, SubscriptionId subId,
                        const SubscriptionRecord& rec)
    {
        auto ev = eventFromPub(pub, publicationId, subId, {});
        for (auto& kv : rec.sessions)
        {
            auto session = kv.second.session.lock();
            if (session)
                session->sendEvent(Event{ev});
        }
    }
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
