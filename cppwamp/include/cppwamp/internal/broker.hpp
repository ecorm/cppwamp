/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_BROKER_HPP
#define CPPWAMP_INTERNAL_BROKER_HPP

#include <cassert>
#include <chrono>
#include <set>
#include <map>
#include <utility>
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../realmobserver.hpp"
#include "../routerconfig.hpp"
#include "../utils/triemap.hpp"
#include "../utils/wildcarduri.hpp"
#include "matchuri.hpp"
#include "random.hpp"
#include "routersession.hpp"

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
        : topicUri_(pub.uri()),
          eligibleSessions_(setOfSessionIds(pub, "eligible")),
          eligibleAuthIds_(setOfStrings(pub, "eligible_authid")),
          eligibleRoles_(setOfStrings(pub, "eligible_authrole")),
          excludedSessions_(setOfSessionIds(pub, "exclude")),
          excludedAuthIds_(setOfStrings(pub, "excluded_authid")),
          excludedRoles_(setOfStrings(pub, "excluded_authrole")),
          publisherId_(publisher->wampId()),
          publicationId_(pid),
          publisherExcluded_(pub.excludeMe())
    {
        Object customOptions;
        bool publisherDisclosed = pub.discloseMe();

        // TODO: Propagate x_foo custom options?
        // https://github.com/wamp-proto/wamp-proto/issues/345
        auto found = pub.options().find("custom");
        if (found != pub.options().end() && found->second.is<Object>())
            customOptions = std::move(found->second.as<Object>());

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

        if (!customOptions.empty())
            event_.withOption("custom", std::move(customOptions));

        hasEligibleList_ =
            !eligibleSessions_.empty() || !eligibleAuthIds_.empty() ||
            !eligibleRoles_.empty();

        hasExcludedList_ =
            !excludedSessions_.empty() || !excludedAuthIds_.empty() ||
            !excludedRoles_.empty();
    }

    // This overload is for meta-events
    BrokerPublication(Pub&& pub, PublicationId pid)
        : topicUri_(pub.uri()),
          event_(Event({}, std::move(pub), nullId(), pid)),
          publicationId_(pid)
    {}

    void setSubscriptionId(SubscriptionId subId)
    {
        event_.setSubscriptionId({}, subId);
    }

    void enableTopicDetail()
    {
        if (!topicDetailEnabled_)
        {
            event_.withOption("topic", topicUri_);
            topicDetailEnabled_ = true;
        }
    }

    bool sendTo(RouterSession& subscriber) const
    {
        bool eligible = isEligible(subscriber);
        if (eligible)
            subscriber.sendEvent(event_);
        return eligible;
    }

    const Uri& topicUri() const {return topicUri_;}

    PublicationId publicationId() const {return publicationId_;}

private:
    static std::set<SessionId> setOfSessionIds(const Pub& pub,
                                               const String& key)
    {
        std::set<SessionId> set;
        const auto& variant = pub.optionByKey(key);
        if (variant.template is<Array>())
        {
            SessionId id;
            for (const auto& element: variant.template as<Array>())
                if (optionToUnsignedInteger(element, id))
                    set.emplace(id);
        }
        return set;
    }

    static std::set<String> setOfStrings(const Pub& pub, const String& key)
    {
        std::set<String> set;
        const auto& variant = pub.optionByKey(key);
        if (variant.template is<Array>())
            for (const auto& element: variant.template as<Array>())
                if (element.is<String>())
                    set.emplace(std::move(element.as<String>()));
        return set;
    }

    bool isEligible(const RouterSession& subscriber) const
    {
        auto id = subscriber.wampId();
        const auto& authId = subscriber.authInfo().id();
        const auto& authRole = subscriber.authInfo().role();

        if (publisherExcluded_ && id == publisherId_)
            return false;

        if (hasExcludedList_)
        {
            if (excludedSessions_.count(id) != 0 ||
                excludedAuthIds_.count(authId) != 0 ||
                excludedRoles_.count(authRole) != 0)
            {
                return false;
            }
        }

        if (!hasEligibleList_)
            return true;
        if (!eligibleSessions_.empty() && eligibleSessions_.count(id) != 0)
            return true;
        if (!eligibleAuthIds_.empty() && eligibleAuthIds_.count(authId) != 0)
            return true;
        if (!eligibleRoles_.empty() && eligibleRoles_.count(authId) != 0)
            return true;
        return false;
    }

    Uri topicUri_;
    Event event_;
    std::set<SessionId> eligibleSessions_;
    std::set<String> eligibleAuthIds_;
    std::set<String> eligibleRoles_;
    std::set<SessionId> excludedSessions_;
    std::set<String> excludedAuthIds_;
    std::set<String> excludedRoles_;
    SessionId publisherId_ = nullId();
    PublicationId publicationId_ = nullId();
    bool publisherExcluded_ = false;
    bool hasEligibleList_ = false;
    bool hasExcludedList_ = false;
    bool topicDetailEnabled_ = false;
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

    BrokerSubscription(MatchUri topic, SubscriptionId subId)
        : topic_(std::move(topic)),
          subId_(subId),
          created_(std::chrono::system_clock::now())
    {}

    bool empty() const {return subscribers_.empty();}

    const MatchUri& topic() const {return topic_;}

    SubscriptionId subscriptionId() const {return subId_;}

    SubscriptionDetails details() const
    {
        std::vector<SessionId> subscribers;
        for (const auto& kv: subscribers_)
            subscribers.push_back(kv.first);
        return {std::move(subscribers),
                {topic_.uri(), created_, subId_, topic_.policy()}};
    }

    void addSubscriber(SessionId sid, BrokerSubscriberInfo info)
    {
        // Does not clobber subscriber info if already subscribed.
        subscribers_.emplace(sid, std::move(info));
    }

    bool removeSubscriber(SessionId sid) {return subscribers_.erase(sid) != 0;}

    std::size_t publish(BrokerPublication& info) const
    {
        std::size_t count = 0;
        info.setSubscriptionId(subId_);
        for (auto& kv : subscribers_)
        {
            auto subscriber = kv.second.session.lock();
            if (subscriber)
                count += info.sendTo(*subscriber);
        }
        return count;
    }

private:
    std::map<SessionId, BrokerSubscriberInfo> subscribers_;
    MatchUri topic_;
    SubscriptionId subId_ = nullId();
    std::chrono::system_clock::time_point created_;
};

//------------------------------------------------------------------------------
// Need associative container with stable iterators
// for use in by-pattern maps.
using BrokerSubscriptionMap = std::map<SubscriptionId, BrokerSubscription>;

//------------------------------------------------------------------------------
class BrokerSubscriptionIdGenerator
{
public:
    SubscriptionId operator()() {return ++nextSubscriptionId_;}

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

    const Uri& topicUri() const {return topic_.uri();}

    MatchPolicy policy() const {return topic_.policy();}

    BrokerSubscription* addNewSubscriptionRecord() &&
    {
        auto subId = subIdGen_();
        BrokerSubscription record{std::move(topic_), subId};
        record.addSubscriber(sessionId_, std::move(subscriber_));
        auto emplaced = subscriptions_.emplace(subId, std::move(record));
        assert(emplaced.second);
        return &(emplaced.first->second);
    }

    void addSubscriberToExistingRecord(BrokerSubscription& record) &&
    {
        record.addSubscriber(sessionId_, std::move(subscriber_));
    }

private:
    MatchUri topic_;
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
    BrokerSubscription* subscribe(BrokerSubscribeRequest& req)
    {
        auto key = req.topicUri();
        BrokerSubscription* record = nullptr;
        auto found = trie_.find(key);
        if (found == trie_.end())
        {
            record = std::move(req).addNewSubscriptionRecord();
            trie_.emplace(std::move(key), record);
        }
        else
        {
            record = iteratorValue(found);
            std::move(req).addSubscriberToExistingRecord(*record);
        }
        return record;
    }

    void erase(const Uri& topicUri) {trie_.erase(topicUri);}

    void removeSubscriber(SessionId sessionId)
    {
        auto iter = trie_.begin();
        auto end = trie_.end();
        while (iter != end)
        {
            BrokerSubscription* record = iteratorValue(iter);
            record->removeSubscriber(sessionId);
            if (record->empty())
                iter = trie_.erase(iter);
            else
                ++iter;
        }
    }

    std::vector<SubscriptionId> listSubscriptions() const
    {
        std::vector<SubscriptionId> subIds;
        auto iter = trie_.begin();
        auto end = trie_.end();
        while (iter != end)
        {
            BrokerSubscription* record = iteratorValue(iter);
            subIds.push_back(record->subscriptionId());
        }
        return subIds;
    }

    template <typename F>
    std::size_t forEachSubscription(F&& functor) const
    {
        auto iter = trie_.begin();
        auto end = trie_.end();
        while (iter != end)
        {
            BrokerSubscription* record = iteratorValue(iter);
            functor(record->details());
        }
        return trie_.size();
    }

    ErrorOr<SubscriptionDetails> lookupSubscription(const Uri& uri) const
    {
        auto found = trie_.find(uri);
        if (found == trie_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        BrokerSubscription* record = iteratorValue(found);
        return record->details();
    }

protected:
    template <typename... Ts>
    BrokerTopicMapBase(Ts&&... trieArgs)
        : trie_(std::forward<Ts>(trieArgs)...)
    {}

    template <typename I>
    static BrokerSubscription* iteratorValue(I iter)
    {
        // tsl::htrie_map iterators don't dereference to a key-value pair
        // like util::TokenTrieMap does.
        return TDerived::iteratorValue(iter);
    }

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

    size_t publish(BrokerPublication& info)
    {
        auto found = trie_.find(info.topicUri());
        if (found == trie_.end())
            return 0;

        const BrokerSubscription* record = found.value();
        return record->publish(info);
    }

    void collectMatches(const Uri& uri,
                        std::vector<SubscriptionId>& subIds) const
    {
        auto found = trie_.find(uri);
        if (found == trie_.end())
            return;
        BrokerSubscription* record = iteratorValue(found);
        subIds.push_back(record->subscriptionId());
    }
};

//------------------------------------------------------------------------------
class BrokerPrefixTopicMap
    : public BrokerTopicMapBase<utils::TrieMap<BrokerSubscription*>,
                                BrokerPrefixTopicMap>
{
public:
    BrokerPrefixTopicMap() : Base(+burstThreshold_) {}

    template <typename I>
    static BrokerSubscription* iteratorValue(I iter)
    {
        return iter.value();
    }

    std::size_t publish(BrokerPublication& info)
    {
        if (trie_.empty())
            return 0;

        using Iter = utils::TrieMap<BrokerSubscription*>::const_iterator;
        std::size_t count = 0;
        info.enableTopicDetail();
        trie_.for_each_prefix_of(
            info.topicUri(),
            [&count, &info] (Iter iter)
            {
                const BrokerSubscription* record = iter.value();
                count += record->publish(info);
            });
        return count;
    }

    void collectMatches(const Uri& uri,
                        std::vector<SubscriptionId>& subIds) const
    {
        if (trie_.empty())
            return;

        using Iter = utils::TrieMap<BrokerSubscription*>::const_iterator;
        trie_.for_each_prefix_of(
            uri,
            [&subIds] (Iter iter)
            {
                const BrokerSubscription* record = iter.value();
                subIds.push_back(record->subscriptionId());
            });
    }

private:
    using Base = BrokerTopicMapBase<utils::TrieMap<BrokerSubscription*>,
                                    BrokerPrefixTopicMap>;

    static constexpr std::size_t burstThreshold_ = 1024;
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

    std::size_t publish(BrokerPublication& info)
    {
        if (trie_.empty())
            return 0;

        auto matches = wildcardMatches(trie_, info.topicUri());
        if (matches.done())
            return 0;

        std::size_t count = 0;
        info.enableTopicDetail();
        while (!matches.done())
        {
            const BrokerSubscription* record = matches.value();
            count += record->publish(info);
            matches.next();
        }
        return count;
    }

    void collectMatches(const Uri& uri,
                        std::vector<SubscriptionId>& subIds) const
    {
        if (trie_.empty())
            return;

        auto matches = wildcardMatches(trie_, uri);
        if (matches.done())
            return;

        while (!matches.done())
        {
            const BrokerSubscription* record = matches.value();
            subIds.push_back(record->subscriptionId());
            matches.next();
        }
    }
};

//------------------------------------------------------------------------------
class Broker
{
public:
    Broker(RandomNumberGenerator64 prng) : pubIdGenerator_(prng) {}

    ErrorOr<SubscriptionId> subscribe(RouterSession::Ptr subscriber, Topic&& t,
                                      RealmObserver::Ptr observer)
    {
        BrokerSubscribeRequest req{std::move(t), subscriber, subscriptions_,
                                   subIdGenerator_};
        BrokerSubscription* sub = nullptr;

        switch (req.policy())
        {
        case Policy::exact:    sub = byExact_.subscribe(req);    break;
        case Policy::prefix:   sub = byPrefix_.subscribe(req);   break;
        case Policy::wildcard: sub = byWildcard_.subscribe(req); break;

        default:
        {
            auto unex = makeUnexpectedError(WampErrc::optionNotAllowed);
            auto error = Error::fromRequest({}, t, unex.value())
                             .withArgs("Unknown match policy");
            subscriber->sendRouterCommand(std::move(error), true);
            return unex;
        }
        }

        if (observer)
            observer->onSubscribe(subscriber->details(), sub->details());

        return sub->subscriptionId();
    }

    ErrorOr<Uri> unsubscribe(RouterSession::Ptr subscriber,
                             SubscriptionId subId, RealmObserver::Ptr observer)
    {
        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        BrokerSubscription& record = found->second;
        auto uri = record.topic().uri();
        bool subscriberRemoved = record.removeSubscriber(subscriber->wampId());

        if (!subscriberRemoved)
        {
            if (record.empty())
                eraseTopic(record.topic(), found);
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        }

        if (observer)
        {
            if (record.empty())
            {
                // Grab needed details before the subscription record is
                // erased from the map.
                auto details = record.details();
                eraseTopic(record.topic(), found);
                observer->onUnsubscribe(subscriber->details(),
                                        std::move(details));
            }
            else
            {
                observer->onUnsubscribe(subscriber->details(),
                                        record.details());
            }
        }
        else if (record.empty())
        {
            eraseTopic(record.topic(), found);
        }

        return uri;
    }

    std::pair<PublicationId, std::size_t>
    publish(RouterSession::Ptr publisher, Pub&& pub)
    {
        BrokerPublication info(std::move(pub), pubIdGenerator_(),
                               std::move(publisher));
        std::size_t count = 0;
        count += byExact_.publish(info);
        count += byPrefix_.publish(info);
        count += byWildcard_.publish(info);
        return std::make_pair(info.publicationId(), count);
    }

    void publishMetaEvent(Pub&& pub)
    {
        BrokerPublication info(std::move(pub), pubIdGenerator_());
        byExact_.publish(info);
        byPrefix_.publish(info);
        byWildcard_.publish(info);
    }

    void removeSubscriber(SessionId sessionId)
    {
        byExact_.removeSubscriber(sessionId);
        byPrefix_.removeSubscriber(sessionId);
        byWildcard_.removeSubscriber(sessionId);

        auto iter = subscriptions_.begin();
        auto end = subscriptions_.end();
        while (iter != end)
        {
            auto& sub = iter->second;
            if (sub.empty())
                iter = subscriptions_.erase(iter);
            else
                ++iter;
        }
    }

    SubscriptionLists listSubscriptions() const
    {
        SubscriptionLists lists;
        lists.exact = byExact_.listSubscriptions();
        lists.prefix = byPrefix_.listSubscriptions();
        lists.wildcard = byWildcard_.listSubscriptions();
        return lists;
    }

    template <typename F>
    std::size_t forEachSubscription(MatchPolicy p, F&& functor) const
    {
        switch (p)
        {
        case MatchPolicy::unknown:
            break;

        case MatchPolicy::exact:
            return byExact_.forEachSubscription(std::forward<F>(functor));

        case MatchPolicy::prefix:
            return byPrefix_.forEachSubscription(std::forward<F>(functor));

        case MatchPolicy::wildcard:
            return byWildcard_.forEachSubscription(std::forward<F>(functor));

        default:
            assert(false && "Unexpected MatchPolicy enumerator");
        }

        return 0;
    }

    ErrorOr<SubscriptionDetails> lookupSubscription(const Uri& uri,
                                                    MatchPolicy p) const
    {
        switch (p)
        {
        case MatchPolicy::unknown:
            break;

        case MatchPolicy::exact:
            return byExact_.lookupSubscription(uri);

        case MatchPolicy::prefix:
            return byPrefix_.lookupSubscription(uri);

        case MatchPolicy::wildcard:
            return byWildcard_.lookupSubscription(uri);

        default:
            assert(false && "Unexpected MatchPolicy enumerator");
        }

        return makeUnexpectedError(WampErrc::noSuchSubscription);
    }

    std::vector<SubscriptionId> matchSubscriptions(const Uri& uri)
    {
        std::vector<SubscriptionId> subIds;
        byExact_.collectMatches(uri, subIds);
        byPrefix_.collectMatches(uri, subIds);
        byWildcard_.collectMatches(uri, subIds);
        return subIds;
    }

    ErrorOr<SubscriptionDetails> getSubscription(SubscriptionId sid)
    {
        auto found = subscriptions_.find(sid);
        if (found == subscriptions_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        return found->second.details();
    }

private:
    using Policy = MatchPolicy;

    void eraseTopic(const MatchUri& topic, BrokerSubscriptionMap::iterator iter)
    {
        subscriptions_.erase(iter);
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
