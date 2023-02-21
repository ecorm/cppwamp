/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_BROKER_HPP
#define CPPWAMP_INTERNAL_BROKER_HPP

#include <cassert>
#include <set>
#include <map>
#include <utility>
#include "../erroror.hpp"
#include "../routerconfig.hpp"
#include "../utils/triemap.hpp"
#include "../utils/wildcarduri.hpp"
#include "matchuri.hpp"
#include "random.hpp"
#include "realmsession.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class BrokerPublication
{
public:
    BrokerPublication(Pub&& pub, PublicationId pid, RealmSession::Ptr publisher)
        : topicUri_(pub.uri()),
          eligibleSessions_(setOfSessionIds(pub, "eligible")),
          eligibleAuthIds_(setOfStrings(pub, "eligible_authid")),
          eligibleRoles_(setOfStrings(pub, "eligible_authrole")),
          excludedSessions_(setOfSessionIds(pub, "excluded")),
          excludedAuthIds_(setOfStrings(pub, "excluded_authid")),
          excludedRoles_(setOfStrings(pub, "excluded_authrole")),
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

        hasEligibleOrExcludedList_ =
            !eligibleSessions_.empty() || !eligibleAuthIds_.empty() ||
            !eligibleRoles_.empty() || !excludedSessions_.empty() ||
            !excludedAuthIds_.empty() || !excludedRoles_.empty();
    }

    void setSubscriptionId(SubscriptionId subId)
    {
        event_.withSubscriptionId(subId);
    }

    void enableTopicDetail()
    {
        event_.withOption("topic", topicUri_);
    }

    void sendTo(RealmSession& subscriber) const
    {
        if (isEligible(subscriber))
            subscriber.sendEvent(Event{event_}, topicUri_);
    }

    const String& topicUri() const {return topicUri_;}

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

    bool isEligible(const RealmSession& subscriber) const
    {
        auto id = subscriber.wampId();
        const auto& authId = subscriber.authInfo().id();
        const auto& authRole = subscriber.authInfo().role();

        if (publisherExcluded_ && id == publisherId_)
            return false;
        if (!hasEligibleOrExcludedList_)
            return true;

        if (excludedSessions_.count(id) != 0)
            return false;
        if (excludedAuthIds_.count(authId) != 0)
            return false;
        if (excludedRoles_.count(authRole) != 0)
            return false;

        if (!eligibleSessions_.empty() && eligibleSessions_.count(id) != 0)
            return false;
        if (!eligibleAuthIds_.empty() && eligibleAuthIds_.count(authId) != 0)
            return false;
        if (!eligibleRoles_.empty() && eligibleRoles_.count(authId) != 0)
            return false;

        return true;
    }

    String topicUri_;
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
    bool hasEligibleOrExcludedList_ = false;
};

//------------------------------------------------------------------------------
struct BrokerSubscriberInfo
{
    RealmSession::WeakPtr session;
};

//------------------------------------------------------------------------------
class BrokerSubscription
{
public:
    BrokerSubscription() = default;

    BrokerSubscription(MatchUri topic, SubscriptionId subId)
        : topic_(std::move(topic))
    {}

    bool empty() const {return sessions_.empty();}

    MatchUri topic() const {return topic_;}

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
    MatchUri topic_;
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
    SubscriptionId operator()() {return ++nextSubscriptionId_;}

private:
    EphemeralId nextSubscriptionId_ = nullId();
};

//------------------------------------------------------------------------------
class BrokerSubscribeRequest
{
public:
    BrokerSubscribeRequest(Topic&& t, RealmSession::Ptr s,
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

    std::error_code check(const UriValidator& uriValidator) const
    {
        if (topic_.policy() == MatchPolicy::unknown)
            return make_error_code(WampErrc::optionNotAllowed);
        if (!uriValidator(topic_.uri(), topic_.policy() != MatchPolicy::exact))
            return make_error_code(WampErrc::invalidUri);
        return {};
    }

    BrokerSubscription* addNewSubscriptionRecord()
    {
        auto subId = subIdGen_();
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
            BrokerSubscription* record = iteratorValue(found);
            subId = record->subscriptionId();
            req.addSubscriberToExistingRecord(*record);
        }
        return subId;
    }

    void erase(const String& topicUri) {trie_.erase(topicUri);}


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

protected:
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
    explicit Broker(RandomNumberGenerator64 prng, UriValidator uriValidator)
        : pubIdGenerator_(prng),
          uriValidator_(uriValidator)
    {}

    ErrorOr<SubscriptionId> subscribe(RealmSession::Ptr subscriber, Topic&& t)
    {
        BrokerSubscribeRequest req{std::move(t), subscriber, subscriptions_,
                                   subIdGenerator_};
        auto ec = req.check(uriValidator_);
        if (ec)
            return makeUnexpected(ec);

        switch (req.policy())
        {
        case Policy::exact:    return byExact_.subscribe(req);
        case Policy::prefix:   return byPrefix_.subscribe(req);
        case Policy::wildcard: return byWildcard_.subscribe(req);
        default: break;
        }

        assert(false && "Unexpected MatchPolicy enumerator");
        return 0;
    }

    ErrorOr<String> unsubscribe(RealmSession::Ptr subscriber,
                                SubscriptionId subId)
    {
        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        BrokerSubscription& record = found->second;
        bool erased = record.removeSubscriber(subscriber->wampId());

        if (record.empty())
        {
            const MatchUri& topic = record.topic();
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
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        return record.topic().uri();
    }

    ErrorOr<PublicationId> publish(RealmSession::Ptr publisher, Pub&& pub)
    {
        if (!uriValidator_(pub.uri(), false))
            return makeUnexpectedError(WampErrc::invalidUri);
        BrokerPublication info(std::move(pub), pubIdGenerator_(),
                               std::move(publisher));
        byExact_.publish(info);
        byPrefix_.publish(info);
        byWildcard_.publish(info);
        return info.publicationId();
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

private:
    using Policy = MatchPolicy;

    BrokerSubscriptionMap subscriptions_;
    BrokerExactTopicMap byExact_;
    BrokerPrefixTopicMap byPrefix_;
    BrokerWildcardTopicMap byWildcard_;
    BrokerSubscriptionIdGenerator subIdGenerator_;
    RandomEphemeralIdGenerator pubIdGenerator_;
    UriValidator uriValidator_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BROKER_HPP
