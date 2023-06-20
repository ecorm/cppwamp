/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_READERSHIP_HPP
#define CPPWAMP_INTERNAL_READERSHIP_HPP

#include <cassert>
#include <map>
#include <utility>
#include "../asiodefs.hpp"
#include "../anyhandler.hpp"
#include "../errorinfo.hpp"
#include "../pubsubinfo.hpp"
#include "../subscription.hpp"
#include "clientcontext.hpp"
#include "slotlink.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class MatchUri
{
public:
    using Policy = MatchPolicy;

    MatchUri() = default;

    explicit MatchUri(Uri uri, Policy p = Policy::unknown)
        : uri_(std::move(uri)),
        policy_(p)
    {}

    explicit MatchUri(const Topic& t) : MatchUri(t.uri(), t.matchPolicy()) {}

    explicit MatchUri(Topic&& t)
        : MatchUri(std::move(t).uri({}), t.matchPolicy())
    {}

    const Uri& uri() const {return uri_;}

    Policy policy() const {return policy_;}

    bool operator==(const MatchUri& rhs) const
    {
        return std::tie(policy_, uri_) == std::tie(rhs.policy_, rhs.uri_);
    }

    bool operator!=(const MatchUri& rhs) const
    {
        return std::tie(policy_, uri_) != std::tie(rhs.policy_, rhs.uri_);
    }

    bool operator<(const MatchUri& rhs) const
    {
        return std::tie(policy_, uri_) < std::tie(rhs.policy_, rhs.uri_);
    }

private:
    Uri uri_;
    Policy policy_ = MatchPolicy::unknown;
};

//------------------------------------------------------------------------------
class SubscriptionRecord
{
public:
    using SlotId = ClientLike::SlotId;
    using EventSlotKey = ClientLike::SubscriptionKey;
    using EventSlot = AnyReusableHandler<void (Event)>;
    using Link = SlotLink<SubscriptionTag, EventSlotKey>;

    struct LinkedSlot
    {
        EventSlot handler;
        Link::Ptr link;
    };

    SubscriptionRecord(MatchUri topic, SubscriptionId subId, SlotId slotId,
                       EventSlot&& handler, ClientContext subscriber,
                       Subscription& subscription)
        : topic_(std::move(topic)),
          subId_(subId)
    {
        auto link = Link::create(std::move(subscriber), {subId, slotId});
        slots_.emplace(slotId, LinkedSlot{std::move(handler), link});
        subscription = Subscription({}, std::move(link));
    }

    Subscription addSlot(SlotId slotId, EventSlot&& handler,
                         ClientContext subscriber)
    {
        auto link = Link::create(subscriber, {subId_, slotId});
        LinkedSlot linkedSlot{std::move(handler), link};
        auto emplaced = slots_.emplace(slotId, std::move(linkedSlot));
        assert(emplaced.second);
        return Subscription{{}, std::move(link)};
    }

    void removeSlot(SlotId slotId) {slots_.erase(slotId);}

    const MatchUri& topic() const {return topic_;}

    bool empty() const {return slots_.empty();}

    void postEvent(const Event& event, AnyIoExecutor& executor) const
    {
        for (const auto& kv: slots_)
            postEventToSlot(event, kv.second, executor);
    }

private:
    static void postEventToSlot(Event event, const LinkedSlot& slot,
                                AnyIoExecutor& executor)
    {
        struct Posted
        {
            Event event;
            LinkedSlot slot;

            void operator()()
            {
                // Copy the publication ID before the Event object
                // gets moved away.
                auto pubId = event.publicationId();

                // The catch clauses are to prevent the publisher crashing
                // subscribers when it passes arguments having incorrect type.
                try
                {
                    assert(event.ready());
                    if (slot.link->armed())
                        slot.handler(std::move(event));
                }
                catch (Error& error)
                {
                    auto subId = slot.link->key().first;
                    error["subscriptionId"] = subId;
                    error["publicationId"] = pubId;
                    slot.link->context().onEventError(std::move(error), subId);
                }
                catch (const error::BadType& e)
                {
                    Error error(e);
                    auto subId = slot.link->key().first;
                    error["subscriptionId"] = subId;
                    error["publicationId"] = pubId;
                    slot.link->context().onEventError(std::move(error), subId);
                }
            }
        };

        auto slotExec = boost::asio::get_associated_executor(slot.handler);
        event.setExecutor({}, slotExec);
        Posted posted{std::move(event), std::move(slot)};
        boost::asio::post(
            executor,
            boost::asio::bind_executor(slotExec, std::move(posted)));
    }

    std::map<SlotId, LinkedSlot> slots_;
    MatchUri topic_;
    SubscriptionId subId_;
};

//------------------------------------------------------------------------------
class Readership
{
public:
    using EventSlotKey = ClientLike::SubscriptionKey;
    using EventSlot = AnyReusableHandler<void (Event)>;

    Readership(AnyIoExecutor exec) : executor_(std::move(exec)) {}

    SubscriptionRecord* findSubscription(const MatchUri& topic)
    {
        assert(topic.policy() != MatchPolicy::unknown);
        auto kv = byTopic_.find(topic);
        return kv == byTopic_.end() ? nullptr : &(kv->second->second);
    }

    Subscription addSubscriber(SubscriptionRecord& record, EventSlot&& handler,
                               ClientContext subscriber)
    {
        return record.addSlot(nextSlotId(), std::move(handler),
                              std::move(subscriber));
    }

    Subscription createSubscription(SubscriptionId subId, MatchUri topic,
                                    EventSlot&& handler,
                                    ClientContext subscriber)
    {
        // Check if the router treats the topic as belonging to an existing
        // subcription.
        auto kv = subscriptions_.find(subId);
        if (kv != subscriptions_.end())
        {
            return addSubscriber(kv->second, std::move(handler),
                                 std::move(subscriber));
        }

        auto slotId = nextSlotId();
        Subscription subscription;
        SubscriptionRecord record{topic, subId, slotId, std::move(handler),
                                  std::move(subscriber), subscription};
        auto emplaced = subscriptions_.emplace(subId, std::move(record));
        assert(emplaced.second);

        auto emplaced2 = byTopic_.emplace(std::move(topic), emplaced.first);
        assert(emplaced2.second);

        return subscription;
    }

    // Returns true if the last local slot was removed from a subscription
    // and the client needs to send an UNSUBSCRIBE message.
    bool unsubscribe(EventSlotKey key)
    {
        auto subId = key.first;
        auto kv = subscriptions_.find(subId);
        if (kv == subscriptions_.end())
            return false;

        auto slotId = key.second;
        auto& record = kv->second;
        record.removeSlot(slotId);
        if (!record.empty())
            return false;

        byTopic_.erase(record.topic());
        subscriptions_.erase(kv);
        return true;
    }

    // Returns true if there are any subscriptions matching the event
    bool onEvent(const Event& event)
    {
        auto found = subscriptions_.find(event.subscriptionId());
        if (found == subscriptions_.end())
            return false;

        const auto& record = found->second;
        assert(!record.empty());
        record.postEvent(event, executor_);
        return true;
    }

    const Uri& lookupTopicUri(SubscriptionId subId)
    {
        static const Uri empty;
        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return empty;
        return found->second.topic().uri();
    }

    void clear()
    {
        byTopic_.clear();
        subscriptions_.clear();
        nextSlotId_ = nullId();
    }

private:
    using SlotId = ClientLike::SlotId;
    using LinkedSlot = SubscriptionRecord::LinkedSlot;
    using SubscriptionMap = std::map<SubscriptionId, SubscriptionRecord>;
    using ByTopic = std::map<MatchUri, SubscriptionMap::iterator>;

    SlotId nextSlotId() {return nextSlotId_++;}

    SubscriptionMap subscriptions_;
    ByTopic byTopic_;
    AnyIoExecutor executor_;
    SlotId nextSlotId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_READERSHIP_HPP
