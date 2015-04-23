/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SUBSCRIPTION_HPP
#define CPPWAMP_SUBSCRIPTION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the Subscription class. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "asyncresult.hpp"
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

    @see ScopedSubscription, Session::subscribe, CoroSession::subscribe */
//------------------------------------------------------------------------------
class Subscription
{
public:
    /** Default constructor */
    Subscription();

    /** Obtains the ID number of this subscription. */
    SubscriptionId id() const;

    /** Unsubscribes from the topic. */
    void unsubscribe() const;

public:
    // Internal use only
    using SlotId = uint64_t;
    using SubscriberPtr = std::weak_ptr<internal::Subscriber>;
    Subscription(SubscriberPtr subscriber, SubscriptionId subId, SlotId slotId,
                 internal::PassKey);
    SlotId slotId(internal::PassKey) const;

private:
    SubscriberPtr subscriber_;
    SubscriptionId subId_ = -1;
    SlotId slotId_ = -1;
};


//------------------------------------------------------------------------------
/** Limits a Subscription's lifetime to a particular scope.

    @see @ref ScopedSubscriptions
    @see Subscription, Session::subscribe, CoroSession::subscribe */
//------------------------------------------------------------------------------
class ScopedSubscription : public Subscription
{
// This class is modeled after boost::signals2::scoped_connection.
public:
    /** Default constructs an empty ScopedSubscription. */
    ScopedSubscription();

    /** Move constructor. */
    ScopedSubscription(ScopedSubscription&& other);

    /** Converting constructor taking a Subscription object to manage. */
    ScopedSubscription(Subscription subscription);

    /** Destructor which automatically unsubscribes the subscription. */
    ~ScopedSubscription();

    /** Move assignment. */
    ScopedSubscription& operator=(ScopedSubscription&& other);

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
