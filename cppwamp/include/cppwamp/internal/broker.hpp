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
#include <mutex>
#include <utility>
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../routerconfig.hpp"
#include "../utils/triemap.hpp"
#include "../utils/wildcarduri.hpp"
#include "metaapi.hpp"
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
            // Disclosed properties are not in the spec, but there is
            // a consensus here:
            // https://github.com/wamp-proto/wamp-proto/issues/57
            const auto& info = publisher->info();
            event_.withOption("publisher", info.sessionId());
            if (!info.auth().id().empty())
                event_.withOption("publisher_authid", info.auth().id());
            if (!info.auth().role().empty())
                event_.withOption("publisher_authrole", info.auth().role());
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
        const auto& authId = subscriber.info().auth().id();
        const auto& authRole = subscriber.info().auth().role();

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

    BrokerSubscription(Uri uri, MatchPolicy policy, SubscriptionId subId)
        : info_(std::move(uri), policy, subId, std::chrono::system_clock::now())
    {}

    bool empty() const {return subscribers_.empty();}

    const SubscriptionInfo& info() const {return info_;}

    SubscriptionInfo info(bool listSubscribers) const
    {
        if (listSubscribers)
            return info_;

        SubscriptionInfo s{info_.uri, info_.matchPolicy, info_.id,
                           info_.created};
        s.subscriberCount = info_.subscriberCount;
        return s;
    }

    void addSubscriber(SessionId sid, BrokerSubscriberInfo info)
    {
        // Does not clobber subscriber info if already subscribed.
        subscribers_.emplace(sid, std::move(info));
        info_.subscribers.insert(sid);
        info_.subscriberCount = info_.subscribers.size();
    }

    bool removeSubscriber(SessionInfo::ConstPtr subscriberInfo,
                          MetaTopics& metaTopics)
    {
        auto sid = subscriberInfo->sessionId();
        auto wasRemoved = subscribers_.erase(sid) != 0;
        info_.subscribers.erase(sid);
        info_.subscriberCount = info_.subscribers.size();
        if (wasRemoved && metaTopics.enabled())
            metaTopics.onUnsubscribe(std::move(subscriberInfo), info(false));
        return wasRemoved;
    }

    std::size_t publish(BrokerPublication& pub,
                        SessionId inhibitedSessionId = 0) const
    {
        std::size_t count = 0;
        pub.setSubscriptionId(info_.id);
        for (auto& kv : subscribers_)
        {
            auto subscriber = kv.second.session.lock();
            if (subscriber && (subscriber->wampId() != inhibitedSessionId))
                count += pub.sendTo(*subscriber);
        }
        return count;
    }

private:
    std::map<SessionId, BrokerSubscriberInfo> subscribers_;
    SubscriptionInfo info_;
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
        : uri_(std::move(t).uri({})),
          subscriber_({s}),
          sessionId_(s->wampId()),
          subscriptions_(subs),
          subIdGen_(gen),
          policy_(t.matchPolicy())
    {}

    const Uri& uri() const {return uri_;}

    MatchPolicy policy() const {return policy_;}

    BrokerSubscription* addNewSubscriptionRecord() &&
    {
        auto subId = subIdGen_();
        BrokerSubscription record{std::move(uri_), policy_, subId};
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
    Uri uri_;
    BrokerSubscriberInfo subscriber_;
    SessionId sessionId_;
    BrokerSubscriptionMap& subscriptions_;
    BrokerSubscriptionIdGenerator& subIdGen_;
    MatchPolicy policy_;
};

//------------------------------------------------------------------------------
template <typename TTrie, typename TDerived>
class BrokerTopicMapBase
{
public:
    BrokerSubscription* subscribe(BrokerSubscribeRequest& req)
    {
        auto key = req.uri();
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

    void removeSubscriber(SessionInfo::ConstPtr subscriberInfo,
                          MetaTopics& metaTopics)
    {
        auto iter = trie_.begin();
        auto end = trie_.end();
        while (iter != end)
        {
            BrokerSubscription* record = iteratorValue(iter);
            record->removeSubscriber(subscriberInfo, metaTopics);
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
            subIds.push_back(record->info().id);
        }
        return subIds;
    }

    template <typename F>
    std::size_t forEachSubscription(F&& functor) const
    {
        auto end = trie_.end();
        std::size_t count = 0;
        for (auto iter = trie_.begin(); iter != end; ++iter)
        {
            BrokerSubscription* record = iteratorValue(iter);
            if (!std::forward<F>(functor)(record->info()))
                break;
            ++count;
        }
        return count;
    }

    ErrorOr<SubscriptionInfo> lookupSubscription(const Uri& uri,
                                                 bool listSubscribers) const
    {
        auto found = trie_.find(uri);
        if (found == trie_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        BrokerSubscription* record = iteratorValue(found);
        return record->info(listSubscribers);

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

    std::size_t publish(BrokerPublication& info,
                        SessionId inhibitedSessionId = 0)
    {
        auto found = trie_.find(info.topicUri());
        if (found == trie_.end())
            return 0;

        const BrokerSubscription* record = found.value();
        return record->publish(info, inhibitedSessionId);
    }

    template <typename F>
    std::size_t forEachMatch(const Uri& uri, F&& functor) const
    {
        auto found = trie_.find(uri);
        if (found == trie_.end())
            return 0;
        BrokerSubscription* record = iteratorValue(found);
        std::forward<F>(functor)(record->info());
        return 1;
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

    std::size_t publish(BrokerPublication& info,
                        SessionId inhibitedSessionId = 0)
    {
        if (trie_.empty())
            return 0;

        using Iter = utils::TrieMap<BrokerSubscription*>::const_iterator;
        std::size_t count = 0;
        info.enableTopicDetail();
        trie_.for_each_prefix_of(
            info.topicUri(),
            [&count, &info, &inhibitedSessionId] (Iter iter)
            {
                const BrokerSubscription* record = iter.value();
                count += record->publish(info, inhibitedSessionId);
            });
        return count;
    }

    template <typename F>
    std::size_t forEachMatch(const Uri& uri, F&& functor) const
    {
        if (trie_.empty())
            return 0;

        PrefixTraverser<F> traverser(std::forward<F>(functor));
        trie_.for_each_prefix_of(uri, traverser);
        return traverser.count;
    }

private:
    template <typename F>
    struct PrefixTraverser
    {
        using Iter = utils::TrieMap<BrokerSubscription*>::const_iterator;

        PrefixTraverser(F&& functor) : functor(std::forward<F>(functor)) {}

        void operator()(Iter iter)
        {
            if (!more)
                return;

            const BrokerSubscription* record = iter.value();
            more = std::forward<F>(functor)(record->info());
            if (more)
                ++count;
        }

        F&& functor;
        std::size_t count = 0;
        bool more = true;
    };

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

    std::size_t publish(BrokerPublication& info,
                        SessionId inhibitedSessionId = 0)
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
            count += record->publish(info, inhibitedSessionId);
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
            subIds.push_back(record->info().id);
            matches.next();
        }
    }

    template <typename F>
    std::size_t forEachMatch(const Uri& uri, F&& functor) const
    {
        if (trie_.empty())
            return 0;

        auto matches = wildcardMatches(trie_, uri);
        if (matches.done())
            return 0;

        std::size_t count = 0;
        while (!matches.done())
        {
            const BrokerSubscription* record = matches.value();
            if (!(std::forward<F>(functor)(record->info())))
                break;
            ++count;
            matches.next();
        }
        return count;
    }
};

//------------------------------------------------------------------------------
class Broker
{
public:
    Broker(RandomNumberGenerator64 prng, MetaTopics::Ptr metaTopics)
        : pubIdGenerator_(prng),
          metaTopics_(std::move(metaTopics))
    {}

    ErrorOr<SubscriptionId> subscribe(RouterSession::Ptr subscriber, Topic&& t)
    {
        BrokerSubscribeRequest req{std::move(t), subscriber, subscriptions_,
                                   subIdGenerator_};
        if (req.policy() == Policy::unknown)
        {
            auto unex = makeUnexpectedError(WampErrc::optionNotAllowed);
            auto error = Error::fromRequest({}, t, unex.value())
                             .withArgs("Unknown match policy");
            subscriber->sendRouterCommand(std::move(error), true);
            return unex;
        }

        BrokerSubscription* sub = nullptr;

        {
            MutexGuard guard{queryMutex_};

            switch (req.policy())
            {
            case Policy::exact:    sub = byExact_.subscribe(req);    break;
            case Policy::prefix:   sub = byPrefix_.subscribe(req);   break;
            case Policy::wildcard: sub = byWildcard_.subscribe(req); break;
            default: assert(false && "Unexpected Policy enumerator"); break;
            }
        }

        if (metaTopics_->enabled() && !isMetaTopic(sub->info().uri))
        {
            metaTopics_->onSubscribe(subscriber->sharedInfo(),
                                     sub->info(false));
        }

        return sub->info().id;
    }

    ErrorOr<Uri> unsubscribe(RouterSession::Ptr subscriber,
                             SubscriptionId subId)
    {
        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);

        BrokerSubscription& record = found->second;
        const auto& uri = record.info().uri;
        auto policy = record.info().matchPolicy;

        {
            MutexGuard guard{queryMutex_};
            bool subscriberRemoved =
                record.removeSubscriber(subscriber->sharedInfo(), *metaTopics_);

            if (record.empty())
                eraseTopic(uri, policy, found);

            if (!subscriberRemoved)
                return makeUnexpectedError(WampErrc::noSuchSubscription);
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

    void publishMetaEvent(Pub&& pub, SessionId inhibitedSessionId)
    {
        BrokerPublication info(std::move(pub), pubIdGenerator_());
        byExact_.publish(info, inhibitedSessionId);
        byPrefix_.publish(info, inhibitedSessionId);
        byWildcard_.publish(info, inhibitedSessionId);
    }

    void removeSubscriber(SessionInfo::ConstPtr subscriberInfo)
    {
        MutexGuard guard{queryMutex_};

        byExact_.removeSubscriber(subscriberInfo, *metaTopics_);
        byPrefix_.removeSubscriber(subscriberInfo, *metaTopics_);
        byWildcard_.removeSubscriber(subscriberInfo, *metaTopics_);

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

    ErrorOr<SubscriptionInfo> getSubscription(SubscriptionId sid,
                                              bool listSubscribers) const
    {
        MutexGuard guard{queryMutex_};
        auto found = subscriptions_.find(sid);
        if (found == subscriptions_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        return found->second.info(listSubscribers);
    }

    ErrorOr<SubscriptionInfo> lookupSubscription(
        const Uri& uri, MatchPolicy p, bool listSubscribers) const
    {
        MutexGuard guard{queryMutex_};

        switch (p)
        {
        case MatchPolicy::unknown:
            break;

        case MatchPolicy::exact:
            return byExact_.lookupSubscription(uri, listSubscribers);

        case MatchPolicy::prefix:
            return byPrefix_.lookupSubscription(uri, listSubscribers);

        case MatchPolicy::wildcard:
            return byWildcard_.lookupSubscription(uri, listSubscribers);

        default:
            assert(false && "Unexpected MatchPolicy enumerator");
        }

        return makeUnexpectedError(WampErrc::noSuchSubscription);
    }

    template <typename F>
    std::size_t forEachSubscription(MatchPolicy p, F&& functor) const
    {
        MutexGuard guard{queryMutex_};

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

    template <typename F>
    std::size_t forEachMatch(const Uri& uri, F&& functor) const
    {
        MutexGuard guard{queryMutex_};
        auto count = byExact_.forEachMatch(uri, std::forward<F>(functor));
        count += byPrefix_.forEachMatch(uri, std::forward<F>(functor));
        count += byWildcard_.forEachMatch(uri, std::forward<F>(functor));
        return count;
    }

private:
    using Policy = MatchPolicy;
    using MutexGuard = std::lock_guard<std::mutex>;

    static bool isMetaTopic(const Uri& uri)
    {
        // https://github.com/wamp-proto/wamp-proto/issues/493
        return uri.rfind("wamp.", 0) == 0;
    }

    void eraseTopic(Uri uri, Policy policy,
                    BrokerSubscriptionMap::iterator iter)
    {
        subscriptions_.erase(iter);
        switch (policy)
        {
        case Policy::exact:
            byExact_.erase(uri);
            break;

        case Policy::prefix:
            byPrefix_.erase(uri);
            break;

        case Policy::wildcard:
            byWildcard_.erase(uri);
            break;

        default:
            assert(false && "Unexpected MatchPolicy enumerator");
            break;
        }
    }

    mutable std::mutex queryMutex_;
    BrokerSubscriptionMap subscriptions_;
    BrokerExactTopicMap byExact_;
    BrokerPrefixTopicMap byPrefix_;
    BrokerWildcardTopicMap byWildcard_;
    BrokerSubscriptionIdGenerator subIdGenerator_;
    RandomEphemeralIdGenerator pubIdGenerator_;
    MetaTopics::Ptr metaTopics_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BROKER_HPP
