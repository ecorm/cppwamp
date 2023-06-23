/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SUBSCRIPTION_HPP
#define CPPWAMP_SUBSCRIPTION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Subscription class. */
//------------------------------------------------------------------------------

#include "api.hpp"
#include "wampdefs.hpp"
#include "internal/passkey.hpp"
#include "internal/slotlink.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Represents a pub/sub event subscription.

    A Subscription is a lightweight object returned by Session::subscribe.
    This objects allows users to unsubscribe the subscription.

    It is always safe to unsubscribe via a Subscription object. If the Session
    or the subscription no longer exists, an unsubscribe operation effectively
    does nothing. Duplicate unsubscribes are safely ignored.

    @see ScopedSubscription */
//------------------------------------------------------------------------------
class CPPWAMP_API Subscription
{
public:
    /** Constructs an empty subscription */
    Subscription();

    /** Returns true if the subscription is still active. */
    explicit operator bool() const;

    /** Obtains the ID number of this subscription. */
    SubscriptionId id() const;

    /** Unsubscribes from the topic. */
    void unsubscribe();

private:
    using Link = internal::SubscriptionLink;
    using Key = Link::Key;

    Link::WeakPtr link_;
    SubscriptionId subId_ = nullId();

public:
    // Internal use only
    Subscription(internal::PassKey, const Link::Ptr& p);
    Key key(internal::PassKey) const;
    void disarm(internal::PassKey);
    bool canUnsubscribe(internal::PassKey,
                        const internal::ClientLike& owner) const;
};


//------------------------------------------------------------------------------
/** Limits a Subscription's lifetime to a particular scope.

    @see @ref ScopedSubscriptions
    @see Subscription, Session::subscribe */
//------------------------------------------------------------------------------
class CPPWAMP_API ScopedSubscription : public Subscription
{
// This class is modeled after boost::signals2::scoped_connection.
public:
    /** Default constructs an empty ScopedSubscription. */
    ScopedSubscription();

    /** Move constructor. */
    ScopedSubscription(ScopedSubscription&& other) noexcept;

    /** Converting constructor taking a Subscription object to manage. */
    ScopedSubscription(Subscription subscription);

    /** Destructor which automatically unsubscribes the subscription. */
    ~ScopedSubscription();

    /** Move assignment. */
    ScopedSubscription& operator=(ScopedSubscription&& other) noexcept;

    /** Assigns another Subscription to manage.
        The old subscription is automatically unsubscribed. */
    ScopedSubscription& operator=(Subscription subscription);

    /** Releases the subscription so that it will no longer be automatically
        unsubscribed if the ScopedSubscription is destroyed or reassigned. */
    void release();

    /** Non-copyable. */
    ScopedSubscription(const ScopedSubscription&) = delete;

    /** Non-copyable. */
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

private:
    using Base = Subscription;
};


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/subscription.inl.hpp"
#endif

#endif // CPPWAMP_SUBSCRIPTION_HPP
