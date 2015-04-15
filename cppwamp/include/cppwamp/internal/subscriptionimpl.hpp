/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SUBSCRIPTIONIMPL_HPP
#define CPPWAMP_INTERNAL_SUBSCRIPTIONIMPL_HPP

#include "../subscription.hpp"
#include "passkey.hpp"
#include "subscriber.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class DynamicSubscription : public wamp::Subscription
{
public:
    using Slot = std::function<void (Event)>;

    static Ptr create(Subscriber::Ptr subscriber, Topic&& topic, Slot&& slot)
    {
        return Ptr(new DynamicSubscription(subscriber, std::move(topic),
                                           std::move(slot)));
    }

protected:
    virtual void invoke(Event&& event, internal::PassKey) override
    {
        slot_(std::move(event));
    }

private:
    DynamicSubscription(SubscriberPtr subscriber, Topic&& topic, Slot&& slot)
        : Subscription(subscriber, std::move(topic)),
          slot_(std::move(slot))
    {}

    Slot slot_;
};

//------------------------------------------------------------------------------
template <typename... TParams>
class StaticSubscription : public wamp::Subscription
{
public:
    using Slot = std::function<void (Event, TParams...)>;

    static Ptr create(Subscriber::Ptr subscriber, Topic&& topic, Slot&& slot)
    {
        return Ptr(new StaticSubscription(subscriber, std::move(topic),
                                          std::move(slot)));
    }

protected:
    virtual void invoke(Event&& event, internal::PassKey) override
    {
        // A copy of event.args() must be made because of unspecified evaluation
        // order: http://stackoverflow.com/questions/15680489
        Array args = event.args();
        wamp::Unmarshall<TParams...>::apply(slot_, args, std::move(event));
    }

private:
    StaticSubscription(SubscriberPtr subscriber, Topic&& topic, Slot&& slot)
        : Subscription(subscriber, std::move(topic)),
          slot_(std::move(slot))
    {}

    Slot slot_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SUBSCRIPTIONIMPL_HPP
