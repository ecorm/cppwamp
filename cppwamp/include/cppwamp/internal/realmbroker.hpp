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
#include "../utils/triemap.hpp"
#include "../utils/wildcarduri.hpp"
#include "idgen.hpp"
#include "routersession.hpp"

// TODO: Publisher Identification
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
          event_(eventFromPub(std::move(pub), pid)),
          publisherId_(publisher->wampId()),
          publicationId_(pid),
          selfPublishEnabled_(pub.optionOr<bool>("exclude_me", false))
    {}

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
        if (selfPublishEnabled_ || (session.wampId() != publisherId_))
            session.sendEvent(Event{event_});
    }

    const String& topicUri() const {return topicUri_;}

    PublicationId publicationId() const {return publicationId_;}

private:
    static Event eventFromPub(Pub&& pub, PublicationId pubId)
    {
        Event ev{pubId};
        if (!pub.args().empty() || !pub.kwargs().empty())
            ev.withArgList(std::move(pub).args());
        if (!pub.kwargs().empty())
            ev.withKwargs(std::move(pub).kwargs());
        return ev;
    }

    String topicUri_;
    Event event_;
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

    Topic::MatchPolicy policy() const {return topic_.policy();}

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
class RealmBroker
{
public:
    ErrorOr<SubscriptionId> subscribe(Topic&& t, RouterSession::Ptr s)
    {
        BrokerSubscribeRequest req{std::move(t), s, subscriptions_,
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

        assert(false && "Unexpected Topic::MatchPolicy enumerator");
        return 0;
    }

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sessionId)
    {
        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        BrokerSubscription& record = found->second;
        bool erased = record.removeSubscriber(sessionId);

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
                assert(false && "Unexpected Topic::MatchPolicy enumerator");
                break;
            }
        }

        if (!erased)
            return makeUnexpectedError(SessionErrc::noSuchSubscription);
        return erased;
    }

    ErrorOr<PublicationId> publish(Pub&& pub, RouterSession::Ptr publisher)
    {
        BrokerPublication info(std::move(pub), pubIdGenerator_(),
                               std::move(publisher));
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
