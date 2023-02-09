/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_BROKER_HPP
#define CPPWAMP_INTERNAL_BROKER_HPP

#include <cassert>
#include <map>
#include <utility>
#include "../erroror.hpp"
#include "../routerconfig.hpp"
#include "../utils/triemap.hpp"
#include "../utils/wildcarduri.hpp"
#include "random.hpp"
#include "routersession.hpp"

// TODO: Publisher Identification override
// TODO: Subscriber include/exclude lists
// TODO: Publication Trust Levels

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class BrokerPublication
{
public:
    BrokerPublication(Pub&& pub, PublicationId pid,
                      RouterSession::Ptr publisher)
        : topicUri_(pub.topic()),
          publisherId_(publisher->wampId()),
          publicationId_(pid),
          publisherExcluded_(pub.excludeMe())
    {
        bool publisherDisclosed = pub.discloseMe();

        event_ = Event({}, std::move(pub), nullId(), pid);

        if (publisherDisclosed)
        {
            // https://github.com/wamp-proto/wamp-proto/issues/57
            const auto& authInfo = publisher->authInfo();
            event_.withOption("publisher", authInfo.sessionId());
            if (!authInfo.id().empty())
                event_.withOption("publisher_authid", authInfo.id());
            if (!authInfo.role().empty())
                event_.withOption("publisher_authrole", authInfo.role());
        }
    }

    void setSubscriptionId(SubscriptionId subId)
    {
        event_.withSubscriptionId(subId);
    }

    void enableTopicDetail()
    {
        event_.withOption("topic", topicUri_);
    }

    void sendTo(RouterSession& session) const
    {
        if (!publisherExcluded_ || (session.wampId() != publisherId_))
            session.sendEvent(Event{event_}, topicUri_);
    }

    const String& topicUri() const {return topicUri_;}

    PublicationId publicationId() const {return publicationId_;}

private:
    String topicUri_;
    Event event_;
    SessionId publisherId_;
    PublicationId publicationId_;
    bool publisherExcluded_;
};

//------------------------------------------------------------------------------
class BrokerUriAndPolicy
{
public:
    using Policy = MatchPolicy;

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
class BrokerSubscription
{
public:
    BrokerSubscription() = default;

    BrokerSubscription(BrokerUriAndPolicy topic, SubscriptionId subId)
        : topic_(std::move(topic))
    {}

    bool empty() const {return sessions_.empty();}

    BrokerUriAndPolicy topic() const {return topic_;}

    SubscriptionId subscriptionId() const {return subId_;}

    void addSubscriber(SessionId sid, BrokerSubscriberInfo info)
    {
        // Does not clobber subscriber info if already subscribed.
        sessions_.emplace(sid, std::move(info));
    }

    bool removeSubscriber(SessionId sid) {return sessions_.erase(sid) != 0;}

    void publish(BrokerPublication& info) const
    {
        info.setSubscriptionId(subId_);
        for (auto& kv : sessions_)
        {
            auto subscriber = kv.second.session.lock();
            if (subscriber)
                info.sendTo(*subscriber);
        }
    }

private:
    std::map<SessionId, BrokerSubscriberInfo> sessions_;
    BrokerUriAndPolicy topic_;
    SubscriptionId subId_;
};

//------------------------------------------------------------------------------
// Need associative container with stable iterators
// for use in by-pattern maps.
using BrokerSubscriptionMap = std::map<SubscriptionId, BrokerSubscription>;

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
class BrokerSubscribeRequest
{
public:
    BrokerSubscribeRequest(Topic&& t, RouterSession::Ptr s,
                           BrokerSubscriptionMap& subs,
                           BrokerSubscriptionIdGenerator& gen)
        : topic_(std::move(t)),
          subscriber_({s}),
          sessionId_(s->wampId()),
          subscriptions_(subs),
          subIdGen_(gen)
    {}

    const String& topicUri() const {return topic_.uri();}

    MatchPolicy policy() const {return topic_.policy();}

    std::error_code check() const
    {
        return topic_.check();
    }

    BrokerSubscription* addNewSubscriptionRecord()
    {
        auto subId = subIdGen_.next(subscriptions_);
        BrokerSubscription record{std::move(topic_), subId};
        record.addSubscriber(sessionId_, std::move(subscriber_));
        auto emplaced = subscriptions_.emplace(subId, std::move(record));
        assert(emplaced.second);
        return &(emplaced.first->second);
    }

    void addSubscriberToExistingRecord(BrokerSubscription& record)
    {
        record.addSubscriber(sessionId_, std::move(subscriber_));
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
    ErrorOr<SubscriptionId> subscribe(BrokerSubscribeRequest& req)
    {
        auto key = req.topicUri();
        SubscriptionId subId = nullId();
        auto found = trie_.find(key);
        if (found == trie_.end())
        {
            BrokerSubscription* record = req.addNewSubscriptionRecord();
            subId = record->subscriptionId();
            trie_.emplace(std::move(key), record);
        }
        else
        {
            // tsl::htrie_map iterators don't dereference to a key-value pair
            // like util::TokenTrieMap does.
            BrokerSubscription* record = TDerived::iteratorValue(found);
            subId = record->subscriptionId();
            req.addSubscriberToExistingRecord(*record);
        }
        return subId;
    }

    void erase(const String& topicUri) {trie_.erase(topicUri);}

protected:
    TTrie trie_;
};

//------------------------------------------------------------------------------
class BrokerExactTopicMap
    : public BrokerTopicMapBase<utils::BasicTrieMap<char, BrokerSubscription*>,
                                BrokerExactTopicMap>
{
public:
    template <typename I>
    static BrokerSubscription* iteratorValue(I iter) {return iter.value();}

    void publish(BrokerPublication& info)
    {
        auto found = trie_.find(info.topicUri());
        if (found != trie_.end())
        {
            const BrokerSubscription* record = found.value();
            record->publish(info);
        }
    }
};

//------------------------------------------------------------------------------
class BrokerPrefixTopicMap
    : public BrokerTopicMapBase<utils::BasicTrieMap<char, BrokerSubscription*>,
                                BrokerPrefixTopicMap>
{
public:
    template <typename I>
    static BrokerSubscription* iteratorValue(I iter)
    {
        return iter.value();
    }

    void publish(BrokerPublication& info)
    {
        auto range = trie_.equal_prefix_range(info.topicUri());
        if (range.first == range.second)
            return;

        info.enableTopicDetail();
        for (; range.first != range.second; ++range.first)
        {
            const BrokerSubscription* record = range.first.value();
            record->publish(info);
        }
    }
};

//------------------------------------------------------------------------------
class BrokerWildcardTopicMap
    : public BrokerTopicMapBase<utils::UriTrieMap<BrokerSubscription*>,
                                BrokerWildcardTopicMap>
{
public:
    template <typename I>
    static BrokerSubscription* iteratorValue(I iter)
    {
        return iter->second;
    }

    void publish(BrokerPublication& info)
    {
        auto matches = wildcardMatches(trie_, info.topicUri());
        if (matches.done())
            return;

        info.enableTopicDetail();
        while (!matches.done())
        {
            const BrokerSubscription* record = matches.value();
            record->publish(info);
            matches.next();
        }
    }
};

//------------------------------------------------------------------------------
class Broker
{
public:
    explicit Broker(RandomNumberGenerator64 prng) : pubIdGenerator_(prng) {}

    ErrorOr<SubscriptionId> subscribe(RouterSession::Ptr subscriber, Topic&& t)
    {
        BrokerSubscribeRequest req{std::move(t), subscriber, subscriptions_,
                                   subIdGenerator_};

        auto ec = req.check();
        if (ec)
            return makeUnexpected(ec);

        switch (req.policy())
        {
        case Policy::unknown:
            return makeUnexpectedError(SessionErrc::optionNotAllowed);

        case Policy::exact:
            return byExact_.subscribe(req);

        case Policy::prefix:
            return byPrefix_.subscribe(req);

        case Policy::wildcard:
            return byWildcard_.subscribe(req);

        default:
            break;
        }

        assert(false && "Unexpected MatchPolicy enumerator");
        return 0;
    }

    ErrorOr<String> unsubscribe(RouterSession::Ptr subscriber,
                                SubscriptionId subId)
    {
        // TODO: Unsubscribe all from subscriber leaving realm

        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        BrokerSubscription& record = found->second;
        bool erased = record.removeSubscriber(subscriber->wampId());

        if (record.empty())
        {
            const BrokerUriAndPolicy& topic = record.topic();
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
                assert(false && "Unexpected MatchPolicy enumerator");
                break;
            }
        }

        if (!erased)
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        return record.topic().uri();
    }

    ErrorOr<PublicationId> publish(RouterSession::Ptr publisher, Pub&& pub)
    {
        BrokerPublication info(std::move(pub), pubIdGenerator_(),
                               std::move(publisher));
        byExact_.publish(info);
        byPrefix_.publish(info);
        byWildcard_.publish(info);
        return info.publicationId();
    }

private:
    using Policy = MatchPolicy;

    BrokerSubscriptionMap subscriptions_;
    BrokerExactTopicMap byExact_;
    BrokerPrefixTopicMap byPrefix_;
    BrokerWildcardTopicMap byWildcard_;
    BrokerSubscriptionIdGenerator subIdGenerator_;
    RandomEphemeralIdGenerator pubIdGenerator_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BROKER_HPP
