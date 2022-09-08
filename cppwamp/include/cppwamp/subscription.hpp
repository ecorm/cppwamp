/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SUBSCRIPTION_HPP
#define CPPWAMP_SUBSCRIPTION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Subscription class. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "api.hpp"
#include "erroror.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class Subscriber; }

//------------------------------------------------------------------------------
/** Represents a pub/sub event subscription.

    A Subscription is a lightweight object returned by the `subscribe` member
    functions of the _Session_ family of classes. This objects allows users to
    unsubscribe the subscription.

    It is always safe to unsubscribe via a Subscription object. If the Session
    or the subscription no longer exists, an unsubscribe operation effectively
    does nothing.

    @see ScopedSubscription, Session::subscribe */
//------------------------------------------------------------------------------
class CPPWAMP_API Subscription
{
public:
    /** Constructs an empty subscription */
    Subscription();

    /** Copy constructor. */
    Subscription(const Subscription& other);

    /** Move constructor. */
    Subscription(Subscription&& other) noexcept;

    /** Returns false if the subscription is empty. */
    explicit operator bool() const;

    /** Obtains the ID number of this subscription. */
    SubscriptionId id() const;

    /** Copy assignment. */
    Subscription& operator=(const Subscription& other);

    /** Move assignment. */
    Subscription& operator=(Subscription&& other) noexcept;

    /** Unsubscribes from the topic. */
    void unsubscribe() const;

private:
    using SubscriberPtr = std::weak_ptr<internal::Subscriber>;
    using SlotId = uint64_t;

    static constexpr SubscriptionId invalidId_ = 0;

    SubscriberPtr subscriber_;
    SubscriptionId subId_ = invalidId_;
    SlotId slotId_ = invalidId_;

public:
    // Internal use only
    Subscription(SubscriberPtr subscriber, SubscriptionId subId, SlotId slotId,
                 internal::PassKey);
    SlotId slotId(internal::PassKey) const;
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
#include "internal/subscription.ipp"
#endif

#endif // CPPWAMP_SUBSCRIPTION_HPP
