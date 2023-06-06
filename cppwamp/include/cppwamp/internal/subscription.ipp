/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../subscription.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

/*******************************************************************************
 * Subscription
*******************************************************************************/

/** @post `bool(*this) == false` */
CPPWAMP_INLINE Subscription::Subscription() {}

CPPWAMP_INLINE Subscription::operator bool() const {return !slot_.expired();}

CPPWAMP_INLINE SubscriptionId Subscription::id() const {return subId_;}

CPPWAMP_INLINE void Subscription::unsubscribe()
{
    auto slot = slot_.lock();
    if (slot)
        slot->remove();
}

CPPWAMP_INLINE Subscription::Subscription(internal::PassKey, TrackedSlotPtr p)
    : slot_(p),
      subId_(p->key().first)
{}

CPPWAMP_INLINE Subscription::Key Subscription::key(internal::PassKey) const
{
    auto slot = slot_.lock();
    return slot ? slot->key() : Key{};
}

CPPWAMP_INLINE void Subscription::disarm(internal::PassKey)
{
    auto slot = slot_.lock();
    if (slot)
        slot->disarm();
}


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
    unsubscribe();
}

CPPWAMP_INLINE ScopedSubscription&
ScopedSubscription::operator=(ScopedSubscription&& other) noexcept
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
