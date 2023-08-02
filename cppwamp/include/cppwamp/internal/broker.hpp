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
#include <memory>
#include <mutex>
#include <utility>
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../routeroptions.hpp"
#include "../traits.hpp"
#include "../uri.hpp"
#include "../utils/triemap.hpp"
#include "../utils/wildcarduri.hpp"
#include "authorizationlistener.hpp"
#include "commandinfo.hpp"
#include "disclosuremode.hpp"
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
                      const RouterSession::Ptr& publisher)
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
        const bool publisherDisclosed = pub.discloseMe();

        // TODO: WAMP - Propagate x_foo custom options?
        // https://github.com/wamp-proto/wamp-proto/issues/345
        auto found = pub.options().find("custom");
        if (found != pub.options().end() && found->second.is<Object>())
            customOptions = std::move(found->second.as<Object>());

        event_ = Event({}, std::move(pub), nullId(), pid);

        if (publisherDisclosed)
        {
            // TODO: WAMP - Disclosed properties are not in the spec, but there
            // is a consensus here:
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
        const bool eligible = isEligible(subscriber);
        if (eligible)
            subscriber.sendEvent(event_);
        return eligible;
    }

    const Uri& topicUri() const {return topicUri_;}

    bool isMeta() const {return topicUri_.rfind("wamp.", 0) == 0;}

    PublicationId publicationId() const {return publicationId_;}

private:
    static std::set<SessionId> setOfSessionIds(const Pub& pub,
                                               const String& key)
    {
        std::set<SessionId> set;
        const auto& variant = pub.optionByKey(key);
        if (variant.template is<Array>())
        {
            SessionId id = 0;
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
        {
            for (const auto& element: variant.template as<Array>())
            {
                if (element.is<String>())
                    set.emplace(element.as<String>());
            }
        }
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

    bool removeSubscriber(const SessionInfo& subscriberInfo,
                          MetaTopics& metaTopics)
    {
        auto sid = subscriberInfo.sessionId();
        auto wasRemoved = subscribers_.erase(sid) != 0;
        info_.subscribers.erase(sid);
        auto subscriberCount = info_.subscribers.size();
        info_.subscriberCount = subscriberCount;
        if (wasRemoved && metaTopics.enabled())
            metaTopics.onUnsubscribe(subscriberInfo, info(false));
        return wasRemoved;
    }

    std::size_t publish(BrokerPublication& pub,
                        SessionId inhibitedSessionId = 0) const
    {
        std::size_t count = 0;
        pub.setSubscriptionId(info_.id);
        for (const auto& kv : subscribers_)
        {
            auto subscriber = kv.second.session.lock();
            if (static_cast<bool>(subscriber) &&
                (subscriber->wampId() != inhibitedSessionId))
            {
                count += (pub.sendTo(*subscriber) ? 1 : 0);
            }
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
    BrokerSubscribeRequest(Topic&& t, const RouterSession::Ptr& s,
                           BrokerSubscriptionMap& subs,
                           BrokerSubscriptionIdGenerator& gen)
        : uri_(std::move(t).uri({})),
          subscriber_({s}),
          sessionId_(s->wampId()),
          subscriptions_(&subs),
          subIdGen_(&gen),
          policy_(t.matchPolicy())
    {}

    const Uri& uri() const {return uri_;}

    MatchPolicy policy() const {return policy_;}

    BrokerSubscription* addNewSubscriptionRecord() &&
    {
        auto subId = subIdGen_->operator()();
        BrokerSubscription record{std::move(uri_), policy_, subId};
        record.addSubscriber(sessionId_, std::move(subscriber_));
        auto emplaced = subscriptions_->emplace(subId, std::move(record));
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
    BrokerSubscriptionMap* subscriptions_ = nullptr;
    BrokerSubscriptionIdGenerator* subIdGen_ = nullptr;
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

    void removeSubscriber(const SessionInfo& subscriberInfo,
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
            if (!functor(record->info()))
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
    explicit BrokerTopicMapBase(Ts&&... trieArgs)
        : trie_(std::forward<Ts>(trieArgs)...)
    {}

    template <typename I>
    static BrokerSubscription* iteratorValue(I iter)
    {
        // tsl::htrie_map iterators don't dereference to a key-value pair
        // like util::TokenTrieMap does.
        return TDerived::iteratorValue(iter);
    }

    TTrie& trie() {return trie_;}

    const TTrie& trie() const {return trie_;}

private:
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
        auto found = trie().find(info.topicUri());
        if (found == trie().end())
            return 0;

        const BrokerSubscription* record = found.value();
        return record->publish(info, inhibitedSessionId);
    }

    template <typename F>
    std::size_t forEachMatch(const Uri& uri, F&& functor) const
    {
        auto found = trie().find(uri);
        if (found == trie().end())
            return 0;
        BrokerSubscription* record = iteratorValue(found);
        functor(record->info());
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
        if (trie().empty())
            return 0;

        if (info.isMeta())
            return publishMeta(info, inhibitedSessionId);

        using Iter = utils::TrieMap<BrokerSubscription*>::const_iterator;
        std::size_t count = 0;
        info.enableTopicDetail();
        trie().for_each_prefix_of(
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
        if (trie().empty())
            return 0;

        PrefixVisitor<Decay<F>> visitor(std::forward<F>(functor));
        trie().for_each_prefix_of(uri, visitor);
        return visitor.count();
    }

private:
    using Base = BrokerTopicMapBase<utils::TrieMap<BrokerSubscription*>,
                                    BrokerPrefixTopicMap>;

    template <typename TFunctor>
    class PrefixVisitor
    {
    public:
        using Iter = utils::TrieMap<BrokerSubscription*>::const_iterator;

        explicit PrefixVisitor(TFunctor f) : functor_(std::move(f)) {}

        std::size_t count() const {return count_;}

        void operator()(Iter iter)
        {
            if (!more_)
                return;

            const BrokerSubscription* record = iter.value();
            more_ = functor_(record->info());
            if (more_)
                ++count_;
        }

    private:
        TFunctor functor_;
        std::size_t count_ = 0;
        bool more_ = true;
    };

    std::size_t publishMeta(BrokerPublication& info,
                            SessionId inhibitedSessionId)
    {
        static constexpr auto minPrefixSize = sizeof("wamp.") - sizeof('\0');

        using Iter = utils::TrieMap<BrokerSubscription*>::const_iterator;
        std::size_t count = 0;
        info.enableTopicDetail();
        trie().for_each_prefix_of(
            info.topicUri(),
            [&count, &info, &inhibitedSessionId] (Iter iter)
            {
                // As a security measure, don't publish if subscription URI
                // does not fully contain the "wamp." prefix.
                const BrokerSubscription* record = iter.value();
                if (record->info().uri.size() >= minPrefixSize)
                    count += record->publish(info, inhibitedSessionId);
            });
        return count;
    }

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
        if (trie().empty())
            return 0;

        if (info.isMeta())
            return publishMeta(info, inhibitedSessionId);

        auto matches = wildcardMatches(trie(), info.topicUri());
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

    template <typename F>
    std::size_t forEachMatch(const Uri& uri, F&& functor) const
    {
        if (trie().empty())
            return 0;

        auto matches = wildcardMatches(trie(), uri);
        if (matches.done())
            return 0;

        std::size_t count = 0;
        while (!matches.done())
        {
            const BrokerSubscription* record = matches.value();
            if (!functor(record->info()))
                break;
            ++count;
            matches.next();
        }
        return count;
    }

private:
    std::size_t publishMeta(BrokerPublication& info,
                            SessionId inhibitedSessionId)
    {
        auto matches = wildcardMatches(trie(), info.topicUri());
        if (matches.done())
            return 0;

        std::size_t count = 0;
        info.enableTopicDetail();
        while (!matches.done())
        {
            // As a security measure, don't publish to subscriptions
            // where the first token is a wildcard.
            if (!matches.key().empty() && !matches.key().front().empty())
            {
                const BrokerSubscription* record = matches.value();
                count += record->publish(info, inhibitedSessionId);
            }
            matches.next();
        }
        return count;
    }
};

//------------------------------------------------------------------------------
class BrokerImpl
{
public:
    BrokerImpl(RandomNumberGenerator64 prng, MetaTopics::Ptr metaTopics,
               Authorizer::Ptr authorizer)
        : pubIdGenerator_(std::move(prng)),
          metaTopics_(std::move(metaTopics)),
          authorizer_(std::move(authorizer))
    {}

    const Authorizer::Ptr& authorizer() const {return authorizer_;}

    void subscribe(const RouterSession::Ptr& subscriber, Topic&& t)
    {
        const RequestId reqId = t.requestId({});
        BrokerSubscribeRequest req{std::move(t), subscriber, subscriptions_,
                                   subIdGenerator_};
        BrokerSubscription* sub = nullptr;

        {
            const MutexGuard guard{queryMutex_};

            switch (req.policy())
            {
            case Policy::exact:    sub = byExact_.subscribe(req);    break;
            case Policy::prefix:   sub = byPrefix_.subscribe(req);   break;
            case Policy::wildcard: sub = byWildcard_.subscribe(req); break;
            default: assert(false && "Unexpected Policy enumerator"); break;
            }
        }

        subscriber->sendRouterCommand(Subscribed{reqId, sub->info().id},
                                      sub->info().uri);

        if (metaTopics_->enabled() && !isMetaTopic(sub->info().uri))
        {
            metaTopics_->onSubscribe(subscriber->sharedInfo(),
                                     sub->info(false));
        }
    }

    void unsubscribe(const RouterSession::Ptr& subscriber,
                     const Unsubscribe& cmd)
    {
        Uri uri;
        auto kv = subscriptions_.find(cmd.subscriptionId());
        bool found = kv != subscriptions_.end();

        if (found)
        {
            BrokerSubscription& record = kv->second;
            uri = record.info().uri;
            auto policy = record.info().matchPolicy;

            {
                const MutexGuard guard{queryMutex_};
                found = record.removeSubscriber(subscriber->sharedInfo(),
                                                *metaTopics_);
                if (record.empty())
                {
                    if (authorizer_)
                        authorizer_->uncacheTopic(record.info());
                    eraseTopic(uri, policy, kv);
                }
            }
        }

        if (found)
        {
            subscriber->sendRouterCommand(Unsubscribed{cmd.requestId({})}, uri);
        }
        else
        {
            subscriber->sendRouterCommandError(cmd,
                                               WampErrc::noSuchSubscription);
        }
    }

    void publish(const RouterSession::Ptr& publisher, Pub&& pub)
    {
        const auto reqId = pub.requestId({});
        const bool wantsAck = pub.wantsAck({});
        BrokerPublication info{std::move(pub), pub.requestId({}), publisher};
        std::size_t count = 0;
        count += byExact_.publish(info);
        count += byPrefix_.publish(info);
        count += byWildcard_.publish(info);
        if (wantsAck)
        {
            publisher->sendRouterCommand(Published{reqId, info.publicationId()},
                                         info.topicUri(), count);
        }
        else
        {
            publisher->report({AccessAction::serverEvent, reqId,
                               info.topicUri(), Object{{"count", count}}});
        }
    }

    void publishMetaEvent(Pub&& pub, SessionId inhibitedSessionId)
    {
        BrokerPublication info(std::move(pub), pubIdGenerator_());
        byExact_.publish(info, inhibitedSessionId);
        byPrefix_.publish(info, inhibitedSessionId);
        byWildcard_.publish(info, inhibitedSessionId);
    }

    void removeSubscriber(const SessionInfo& subscriberInfo)
    {
        const MutexGuard guard{queryMutex_};

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
        const MutexGuard guard{queryMutex_};
        auto found = subscriptions_.find(sid);
        if (found == subscriptions_.end())
            return makeUnexpectedError(WampErrc::noSuchSubscription);
        return found->second.info(listSubscribers);
    }

    ErrorOr<SubscriptionInfo> lookupSubscription(
        const Uri& uri, MatchPolicy p, bool listSubscribers) const
    {
        const MutexGuard guard{queryMutex_};

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
        const MutexGuard guard{queryMutex_};

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
        const MutexGuard guard{queryMutex_};
        auto count = byExact_.forEachMatch(uri, std::forward<F>(functor));
        count += byPrefix_.forEachMatch(uri, functor);
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

    void eraseTopic(const Uri& uri, Policy policy,
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
    Authorizer::Ptr authorizer_;
};

//------------------------------------------------------------------------------
class Broker : public std::enable_shared_from_this<Broker>,
               public AuthorizationListener
{
public:
    using Ptr = std::shared_ptr<Broker>;
    using SharedStrand = std::shared_ptr<IoStrand>;

    Broker(AnyIoExecutor exec, SharedStrand strand, RandomNumberGenerator64 prng,
           MetaTopics::Ptr metaTopics, UriValidator::Ptr uriValidator,
           const RealmOptions& opts)
        : impl_(std::move(prng), std::move(metaTopics), opts.authorizer()),
          executor_(std::move(exec)),
          strand_(std::move(strand)),
          uriValidator_(std::move(uriValidator)),
          publisherDisclosure_(opts.publisherDisclosure()),
          metaTopicPublicationAllowed_(opts.metaTopicPublicationAllowed())
    {}

    void subscribe(const RouterSession::Ptr& subscriber, Topic&& topic)
    {
        dispatchCommand(subscriber, std::move(topic));
    }

    void unsubscribe(const RouterSession::Ptr& subscriber, Unsubscribe&& cmd)
    {
        dispatchCommand(subscriber, std::move(cmd));
    }

    void publish(const RouterSession::Ptr& publisher, Pub&& publish)
    {
        dispatchCommand(publisher, std::move(publish));
    }

    void publishMetaEvent(Pub&& pub, SessionId inhibitedSessionId)
    {
        impl_.publishMetaEvent(std::move(pub), inhibitedSessionId);
    }

    void removeSubscriber(const SessionInfo& subscriberInfo)
    {
        impl_.removeSubscriber(subscriberInfo);
    }

    ErrorOr<SubscriptionInfo> getSubscription(SubscriptionId sid,
                                              bool listSubscribers) const
    {
        return impl_.getSubscription(sid, listSubscribers);
    }

    ErrorOr<SubscriptionInfo> lookupSubscription(
        const Uri& uri, MatchPolicy p, bool listSubscribers) const
    {
        return impl_.lookupSubscription(uri, p, listSubscribers);
    }

    template <typename F>
    std::size_t forEachSubscription(MatchPolicy p, F&& functor) const
    {
        return impl_.forEachSubscription(p, std::forward<F>(functor));
    }

    template <typename F>
    std::size_t forEachMatch(const Uri& uri, F&& functor) const
    {
        return impl_.forEachMatch(uri, std::forward<F>(functor));
    }

private:
    template <typename C, typename... Ts>
    static void sendCommandErrorToOriginator(
        RouterSession& originator, const C& cmd, WampErrc errc, Ts&&... args)
    {
        auto error = Error::fromRequest({}, cmd, errc)
                         .withArgs(std::forward<Ts>(args)...);
        originator.sendRouterCommand(std::move(error), true);
    }

    template <typename C>
    void dispatchCommand(RouterSession::Ptr originator, C&& command)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr o;
            ValueTypeOf<C> c;
            void operator()() {self->processCommand(o, std::move(c));}
        };

        boost::asio::dispatch(
            *strand_,
            Dispatched{shared_from_this(), std::move(originator),
                       std::forward<C>(command)});
    }

    void processCommand(const RouterSession::Ptr& subscriber, Topic&& subscribe)
    {
        if (subscribe.matchPolicy() == MatchPolicy::unknown)
        {
            subscriber->sendRouterCommandError(
                subscribe, WampErrc::optionNotAllowed, "Unknown match policy");
            return;
        }

        const bool isPattern = subscribe.matchPolicy() != MatchPolicy::exact;
        if (!uriValidator_->checkTopic(subscribe.uri(), isPattern))
            return subscriber->abort({WampErrc::invalidUri});

        authorize(subscriber, subscribe);
    }

    void processCommand(const RouterSession::Ptr& subscriber, Unsubscribe&& cmd)
    {
        impl_.unsubscribe(subscriber, cmd);
    }

    void processCommand(const RouterSession::Ptr& publisher, Pub&& publish)
    {
        if (!uriValidator_->checkTopic(publish.uri(), false))
            return publisher->abort({WampErrc::invalidUri});
        if (!checkMetaTopicPublicationAttempt(*publisher, publish))
            return;
        authorize(publisher, publish);
    }

    template <typename C>
    void authorize(const RouterSession::Ptr& originator, C& command)
    {
        const auto& authorizer = impl_.authorizer();
        if (!authorizer)
            return bypassAuthorization(originator, std::move(command));

        AuthorizationRequest r{{}, shared_from_this(), originator,
                               authorizer, publisherDisclosure_.disclosure()};
        authorizer->authorize(std::forward<C>(command), std::move(r));
    }

    void bypassAuthorization(const RouterSession::Ptr& subscriber, Topic&& t)
    {
        impl_.subscribe(subscriber, std::move(t));
    }

    void bypassAuthorization(const RouterSession::Ptr& publisher, Pub&& pub)
    {
        const bool disclosed = publisherDisclosure_.compute(pub.disclosed({}),
                                                            false);
        pub.setDisclosed({}, disclosed);
        impl_.publish(publisher, std::move(pub));
    }

    void onAuthorized(const RouterSession::Ptr& subscriber,
                      Topic&& topic) override
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr subscriber;
            Topic topic;

            void operator()()
            {
                self->impl_.subscribe(subscriber, std::move(topic));
            }
        };

        boost::asio::dispatch(
            *strand_,
            Dispatched{shared_from_this(), subscriber, std::move(topic)});
    };

    void onAuthorized(const RouterSession::Ptr& publisher, Pub&& pub) override
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr publisher;
            Pub pub;
            void operator()() {self->impl_.publish(publisher, std::move(pub));}
        };

        boost::asio::dispatch(
            *strand_,
            Dispatched{shared_from_this(), publisher, std::move(pub)});
    };

    bool checkMetaTopicPublicationAttempt(RouterSession& publisher,
                                          const Pub& pub) const
    {
        if (metaTopicPublicationAllowed_ || !pub.isMeta())
            return true;

        sendCommandErrorToOriginator(publisher, pub, WampErrc::invalidUri);
        return false;
    }

    BrokerImpl impl_;
    AnyIoExecutor executor_;
    SharedStrand strand_;
    UriValidator::Ptr uriValidator_;
    DisclosureMode publisherDisclosure_;
    bool metaTopicPublicationAllowed_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BROKER_HPP
