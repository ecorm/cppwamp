/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../subscription.hpp"
#include <utility>
#include "../api.hpp"
#include "subscriber.hpp"

namespace wamp
{

/*******************************************************************************
 * Subscription
*******************************************************************************/

/** @post `!!(*this) == false` */
CPPWAMP_INLINE Subscription::Subscription() {}

CPPWAMP_INLINE Subscription::Subscription(const Subscription& other)
    : subscriber_(other.subscriber_),
      subId_(other.subId_),
      slotId_(other.slotId_)
{}

/** @post `!other == true` */
CPPWAMP_INLINE Subscription::Subscription(Subscription&& other) noexcept
    : subscriber_(other.subscriber_),
      subId_(other.subId_),
      slotId_(other.slotId_)
{
    other.subscriber_.reset();
    other.subId_ = invalidId_;
    other.slotId_ = invalidId_;
}

CPPWAMP_INLINE Subscription::operator bool() const
{
    return subId_ != invalidId_;
}

CPPWAMP_INLINE SubscriptionId Subscription::id() const {return subId_;}

CPPWAMP_INLINE Subscription& Subscription::operator=(const Subscription& other)
{
    subscriber_ = other.subscriber_;
    subId_ = other.subId_;
    slotId_ = other.slotId_;
    return *this;
}

/** @post `!other == true` */
CPPWAMP_INLINE Subscription&
Subscription::operator=(Subscription&& other) noexcept
{
    subscriber_ = other.subscriber_;
    subId_ = other.subId_;
    slotId_ = other.slotId_;
    other.subscriber_.reset();
    other.subId_ = invalidId_;
    other.slotId_ = invalidId_;
    return *this;
}

CPPWAMP_INLINE void Subscription::unsubscribe() const
{
    auto subscriber = subscriber_.lock();
    if (subscriber)
        subscriber->unsubscribe(*this);
}

CPPWAMP_INLINE void Subscription::unsubscribe(ThreadSafe) const
{
    auto subscriber = subscriber_.lock();
    if (subscriber)
        subscriber->safeUnsubscribe(*this);
}

CPPWAMP_INLINE Subscription::Subscription(
    internal::PassKey, SubscriberPtr subscriber, SubscriptionId subId,
    SlotId slotId)
    : subscriber_(subscriber),
      subId_(subId),
      slotId_(slotId)
{}

CPPWAMP_INLINE Subscription::SlotId
Subscription::slotId(internal::PassKey) const {return slotId_;}


/*******************************************************************************
 * ScopedSubscription
*******************************************************************************/

CPPWAMP_INLINE ScopedSubscription::ScopedSubscription() {}

CPPWAMP_INLINE
ScopedSubscription::ScopedSubscription(ScopedSubscription&& other) noexcept
    : Base(std::move(other))
{}

CPPWAMP_INLINE
ScopedSubscription::ScopedSubscription(Subscription subscription)
    : Base(std::move(subscription))
{}

CPPWAMP_INLINE ScopedSubscription::~ScopedSubscription()
{
    unsubscribe(threadSafe);
}

CPPWAMP_INLINE ScopedSubscription&
ScopedSubscription::operator=(ScopedSubscription&& other) noexcept
{
    unsubscribe(threadSafe);
    Base::operator=(std::move(other));
    return *this;
}

CPPWAMP_INLINE ScopedSubscription&
ScopedSubscription::operator=(Subscription subscription)
{
    unsubscribe(threadSafe);
    Base::operator=(std::move(subscription));
    return *this;
}

CPPWAMP_INLINE void ScopedSubscription::release()
{
    Base::operator=(Subscription());
}

} // namespace wamp
