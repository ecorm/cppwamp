/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_REALMBROKER_HPP
#define CPPWAMP_INTERNAL_REALMBROKER_HPP

#include <cassert>
#include <map>
#include <utility>
#include "../erroror.hpp"
#include "../utils/trie.hpp"
#include "../utils/wildcarduri.hpp"
#include "idgen.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class BrokerPublicationInfo
{
public:
    BrokerPublicationInfo(const Pub& p, SessionId sid, PublicationId pid)
        : event_(eventFromPub(p, pid)),
          pub_(p),
          publisherId_(sid),
          publicationId_(pid),
          selfPublishEnabled_(p.optionOr<bool>("exclude_me", false))
    {}

    void setSubscriptionId(SubscriptionId subId)
    {
        event_.withSubscriptionId(subId);
    }

    void enableTopicDetail()
    {
        event_.withOption("topic", pub_.topic());
    }

    void publishTo(RouterSession& session) const
    {
        if (selfPublishEnabled_ || (session.wampId() != publisherId_))
            session.sendEvent(Event{event_});
    }

    const String& topicUri() const {return pub_.topic();}

    PublicationId publicationId() const {return publicationId_;}

private:
    static Event eventFromPub(const Pub& pub, PublicationId pubId)
    {
        Event ev{pubId};
        if (!pub.args().empty() || !pub.kwargs().empty())
            ev.withArgList(pub.args());
        if (!pub.kwargs().empty())
            ev.withKwargs(pub.kwargs());
        return ev;
    }

    Event event_;
    const Pub& pub_;
    SessionId publisherId_;
    PublicationId publicationId_;
    bool selfPublishEnabled_;
};

//------------------------------------------------------------------------------
class BrokerUriAndPolicy
{
public:
    using Policy = Topic::MatchPolicy;

    BrokerUriAndPolicy() = default;

    explicit BrokerUriAndPolicy(String uri, Policy p = Policy::unknown)
        : uri_(std::move(uri)),
          policy_(p)
    {}

    explicit BrokerUriAndPolicy(Topic&& t)
        : BrokerUriAndPolicy(std::move(t).uri({}), t.matchPolicy())
    {}

    const String& uri() const {return uri_;}

    Policy policy() const {return policy_;}

    std::error_code check() const
    {
        // TODO: Check valid URI
        return {};
    }

private:
    String uri_;
    Policy policy_;
};

//------------------------------------------------------------------------------
struct BrokerSubscriberInfo
{
    RouterSession::WeakPtr session;
};

//------------------------------------------------------------------------------
class BrokerSubscriptionRecord
{
public:
    BrokerSubscriptionRecord() = default;

    BrokerSubscriptionRecord(BrokerUriAndPolicy topic)
        : topic_(std::move(topic))
    {}

    bool empty() const {return sessions_.empty();}

    BrokerUriAndPolicy topic() const {return topic_;}

    void addSubscriber(SessionId sid, BrokerSubscriberInfo info)
    {
        sessions_.emplace(sid, std::move(info));
    }

    bool removeSubscriber(SessionId sid) {return sessions_.erase(sid) != 0;}

    void publish(BrokerPublicationInfo& info, SubscriptionId subId) const
    {
        info.setSubscriptionId(subId);
        for (auto& kv : sessions_)
        {
            auto session = kv.second.session.lock();
            if (session)
                info.publishTo(*session);
        }
    }

private:
    std::map<SessionId, BrokerSubscriberInfo> sessions_;
    BrokerUriAndPolicy topic_;
};

//------------------------------------------------------------------------------
class RealmBroker
{
public:
    ErrorOr<SubscriptionId> subscribe(Topic&& t, RouterSession::Ptr s)
    {
        BrokerUriAndPolicy topic{std::move(t)};
        auto topicError = topic.check();
        if (topicError)
            return makeUnexpected(topicError);
        SessionId sid = s->wampId();
        BrokerSubscriberInfo info{std::move(s)};

        switch (topic.policy())
        {
        case Policy::unknown:
            return makeUnexpectedError(SessionErrc::optionNotAllowed);

        case Policy::exact:
        {
            auto key = topic.uri();
            return doSubscribe(byExact_, std::move(key), std::move(topic),
                               std::move(info), sid);
        }

        case Policy::prefix:
        {
            auto key = topic.uri();
            return doSubscribe(byPrefix_, std::move(key), std::move(topic),
                               std::move(info), sid);
        }

        case Policy::wildcard:
        {
            return doSubscribe(byWildcard_, topic.uri(), std::move(topic),
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
        BrokerSubscriptionRecord& rec = found->second;
        bool erased = rec.removeSubscriber(sessionId);

        if (rec.empty())
        {
            const BrokerUriAndPolicy& topic = rec.topic();
            subscriptions_.erase(found);
            switch (topic.policy())
            {
            case Policy::exact:
                byExact_.erase(topic.uri());
                break;

            case Policy::prefix:
                byPrefix_.erase(topic.uri());
                break;

            case Policy::wildcard:
                byWildcard_.erase(topic.uri());
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
        BrokerPublicationInfo info(pub, publisherId, pubIdGenerator_());
        publishExactMatches(info);
        publishPrefixMatches(info);
        publishWildcardMatches(info);
        return info.publicationId();
    }

private:
    using Policy = Topic::MatchPolicy;

    // Need associative container with stable iterators
    // for use in by-pattern maps.
    using SubscriptionMap = std::map<SubscriptionId, BrokerSubscriptionRecord>;

    static bool startsWith(const String& s, const String& prefix)
    {
        // https://stackoverflow.com/a/40441240/245265
        return s.rfind(prefix, 0) == 0;
    }

    template <typename TTrie, typename TKey>
    ErrorOr<SubscriptionId> doSubscribe(
        TTrie& trie, TKey&& key, BrokerUriAndPolicy&& topic,
        BrokerSubscriberInfo&& info, SessionId sessionId)
    {
        SubscriptionId subId = nullId();
        auto found = trie.find(key);
        if (found == trie.end())
        {
            subId = nextSubId();
            auto uri = topic.uri();
            BrokerSubscriptionRecord rec{std::move(topic)};
            rec.addSubscriber(sessionId, std::move(info));
            auto emplaced = subscriptions_.emplace(subId, std::move(rec));
            assert(emplaced.second);
            trie.emplace(std::move(key), emplaced.first);
        }
        else
        {
            subId = found.value()->first;
            BrokerSubscriptionRecord& rec = found.value()->second;
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

    void publishExactMatches(BrokerPublicationInfo& info)
    {
        auto found = byExact_.find(info.topicUri());
        if (found != byExact_.end())
        {
            SubscriptionId subId = (*found)->first;
            const BrokerSubscriptionRecord& rec = (*found)->second;
            rec.publish(info, subId);
        }
    }

    void publishPrefixMatches(BrokerPublicationInfo& info)
    {
        auto range = byPrefix_.equal_prefix_range(info.topicUri());
        if (range.first == range.second)
            return;

        info.enableTopicDetail();
        for (; range.first != range.second; ++range.first)
        {
            SubscriptionId subId = (*range.first)->first;
            const BrokerSubscriptionRecord& rec = (*range.first)->second;
            rec.publish(info, subId);
        }
    }

    void publishWildcardMatches(BrokerPublicationInfo& info)
    {
        auto matches = wildcardMatches(byWildcard_, info.topicUri());
        if (matches.done())
            return;

        info.enableTopicDetail();
        while (!matches.done())
        {
            SubscriptionId subId = matches.value()->first;
            const BrokerSubscriptionRecord& rec = matches.value()->second;
            rec.publish(info, subId);
            matches.next();
        }
    }

    SubscriptionMap subscriptions_;
    utils::BasicTrieMap<char, SubscriptionMap::iterator> byExact_;
    utils::BasicTrieMap<char, SubscriptionMap::iterator> byPrefix_;
    utils::UriTrie<SubscriptionMap::iterator> byWildcard_;
    EphemeralId nextSubscriptionId_ = nullId();
    RandomIdGenerator pubIdGenerator_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REALMBROKER_HPP
