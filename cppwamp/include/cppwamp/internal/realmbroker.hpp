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
// Need associative container with stable iterators
// for use in by-pattern maps.
using BrokerSubscriptionMap =
    std::map<SubscriptionId, BrokerSubscriptionRecord>;

//------------------------------------------------------------------------------
class BrokerSubscriptionIdGenerator
{
public:
    SubscriptionId next(BrokerSubscriptionMap& subscriptions)
    {
        auto s = nextSubscriptionId_;
        while ((s == nullId()) || (subscriptions.count(s) == 1))
            ++s;
        nextSubscriptionId_ = s + 1;
        return s;
    }

private:
    EphemeralId nextSubscriptionId_ = nullId();
};

//------------------------------------------------------------------------------
class BrokerSubscribeInfo
{
public:
    BrokerSubscribeInfo(Topic&& t, RouterSession::Ptr s,
                        BrokerSubscriptionMap& subs,
                        BrokerSubscriptionIdGenerator& gen)
        : topic_(std::move(t)),
          subscriber_({s}),
          sessionId_(s->wampId()),
          subscriptions_(subs),
          subIdGen_(gen)
    {}

    const String& topicUri() const {return topic_.uri();}

    Topic::MatchPolicy policy() const {return topic_.policy();}

    std::error_code check() const
    {
        return topic_.check();
    }

    BrokerSubscriptionMap::iterator addNewSubscriptionRecord()
    {
        auto subId = subIdGen_.next(subscriptions_);
        BrokerSubscriptionRecord rec{std::move(topic_)};
        rec.addSubscriber(sessionId_, std::move(subscriber_));
        auto emplaced = subscriptions_.emplace(subId, std::move(rec));
        assert(emplaced.second);
        return emplaced.first;
    }

    void addSubscriberToExistingRecord(BrokerSubscriptionRecord& rec)
    {
        rec.addSubscriber(sessionId_, std::move(subscriber_));
    }

private:
    BrokerUriAndPolicy topic_;
    BrokerSubscriberInfo subscriber_;
    SessionId sessionId_;
    BrokerSubscriptionMap& subscriptions_;
    BrokerSubscriptionIdGenerator& subIdGen_;
};

//------------------------------------------------------------------------------
template <typename TTrie, typename TDerived>
class BrokerTopicMapBase
{
public:
    ErrorOr<SubscriptionId> subscribe(BrokerSubscribeInfo& info)
    {
        auto key = info.topicUri();
        SubscriptionId subId = nullId();
        auto found = trie_.find(key);
        if (found == trie_.end())
        {
            auto subscriptionMapIter = info.addNewSubscriptionRecord();
            subId = subscriptionMapIter->first;
            trie_.emplace(std::move(key), subscriptionMapIter);
        }
        else
        {
            // tsl::htrie_map iterators don't dereference to a key-value pair
            // like util::TokenTrie does.
            auto subscriptionMapIter = TDerived::iteratorValue(found);

            subId = subscriptionMapIter->first;
            auto& subscriptionRecord = subscriptionMapIter->second;
            info.addSubscriberToExistingRecord(subscriptionRecord);
        }
        return subId;
    }

    void erase(const String& topicUri) {trie_.erase(topicUri);}

protected:
    TTrie trie_;
};

//------------------------------------------------------------------------------
class BrokerExactTopicMap
    : public BrokerTopicMapBase<
          utils::BasicTrieMap<char, BrokerSubscriptionMap::iterator>,
          BrokerExactTopicMap>
{
public:
    template <typename I>
    static BrokerSubscriptionMap::iterator iteratorValue(I iter)
    {
        return iter.value();
    }

    void publish(BrokerPublicationInfo& info)
    {
        auto found = trie_.find(info.topicUri());
        if (found != trie_.end())
        {
            SubscriptionId subId = (*found)->first;
            const BrokerSubscriptionRecord& rec = (*found)->second;
            rec.publish(info, subId);
        }
    }
};

//------------------------------------------------------------------------------
class BrokerPrefixTopicMap
    : public BrokerTopicMapBase<
          utils::BasicTrieMap<char, BrokerSubscriptionMap::iterator>,
          BrokerPrefixTopicMap>
{
public:
    template <typename I>
    static BrokerSubscriptionMap::iterator iteratorValue(I iter)
    {
        return iter.value();
    }

    void publish(BrokerPublicationInfo& info)
    {
        auto range = trie_.equal_prefix_range(info.topicUri());
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
};

//------------------------------------------------------------------------------
class BrokerWildcardTopicMap
    : public BrokerTopicMapBase<
          utils::UriTrie<BrokerSubscriptionMap::iterator>,
          BrokerWildcardTopicMap>
{
public:
    template <typename I>
    static BrokerSubscriptionMap::iterator iteratorValue(I iter)
    {
        return iter->second;
    }

    void publish(BrokerPublicationInfo& info)
    {
        auto matches = wildcardMatches(trie_, info.topicUri());
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
};

//------------------------------------------------------------------------------
class RealmBroker
{
public:
    ErrorOr<SubscriptionId> subscribe(Topic&& t, RouterSession::Ptr s)
    {
        BrokerSubscribeInfo info{std::move(t), s, subscriptions_,
                                 subIdGenerator_};

        auto ec = info.check();
        if (ec)
            return makeUnexpected(ec);

        switch (info.policy())
        {
        case Policy::unknown:
            return makeUnexpectedError(SessionErrc::optionNotAllowed);

        case Policy::exact:
            return byExact_.subscribe(info);

        case Policy::prefix:
            return byPrefix_.subscribe(info);

        case Policy::wildcard:
            return byWildcard_.subscribe(info);

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
        byExact_.publish(info);
        byPrefix_.publish(info);
        byWildcard_.publish(info);
        return info.publicationId();
    }

private:
    using Policy = Topic::MatchPolicy;

    BrokerSubscriptionMap subscriptions_;
    BrokerExactTopicMap byExact_;
    BrokerPrefixTopicMap byPrefix_;
    BrokerWildcardTopicMap byWildcard_;
    BrokerSubscriptionIdGenerator subIdGenerator_;
    RandomIdGenerator pubIdGenerator_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REALMBROKER_HPP
