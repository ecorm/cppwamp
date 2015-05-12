/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "config.hpp"
#include "subscriber.hpp"

namespace wamp
{

/*******************************************************************************
 * Subscription
*******************************************************************************/

/** @post `!!(*this) == false` */
CPPWAMP_INLINE Subscription::Subscription() {}

CPPWAMP_INLINE Subscription::operator bool() const
{
    return subId_ != invalidId_;
}

CPPWAMP_INLINE SubscriptionId Subscription::id() const {return subId_;}

CPPWAMP_INLINE void Subscription::unsubscribe() const
{
    auto subscriber = subscriber_.lock();
    if (subscriber)
        subscriber->unsubscribe(*this);
}

CPPWAMP_INLINE Subscription::Subscription(SubscriberPtr subscriber,
        SubscriptionId subId, SlotId slotId, internal::PassKey)
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
ScopedSubscription::ScopedSubscription(ScopedSubscription&& other)
    : Base(std::move(other))
{}

CPPWAMP_INLINE
ScopedSubscription::ScopedSubscription(Subscription subscription)
    : Base(std::move(subscription))
{}

CPPWAMP_INLINE ScopedSubscription::~ScopedSubscription()
{
    unsubscribe();
}

CPPWAMP_INLINE ScopedSubscription&
ScopedSubscription::operator=(ScopedSubscription&& other)
{
    unsubscribe();
    Base::operator=(std::move(other));
    return *this;
}

CPPWAMP_INLINE ScopedSubscription&
ScopedSubscription::operator=(Subscription subscription)
{
    unsubscribe();
    Base::operator=(std::move(subscription));
    return *this;
}

CPPWAMP_INLINE void ScopedSubscription::release()
{
    Base::operator=(Subscription());
}

} // namespace wamp
