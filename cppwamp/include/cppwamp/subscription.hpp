/*------------------------------------------------------------------------------
            Copyright Emile Cormier, Butterfly Energy Systems, 2014.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SUBSCRIPTION_HPP
#define CPPWAMP_SUBSCRIPTION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the Subscription handle. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "asyncresult.hpp"
#include "wampdefs.hpp"

namespace wamp
{

// Forward declarations
namespace internal
{
    template <typename, typename> class ClientImpl;
    class SubscriptionBase;
}

//------------------------------------------------------------------------------
/** Reference-counting handle used to manage the lifetime of a pub/sub
    event subscription.
    Subscription handles are returned by the `subscribe` member function of the
    client family of classes (see Client::subscribe, CoroClient::subscribe,
    CoroErrcClient::subscribe). These handles point to an underlying event
    subscription object managed by a client. This underlying subscription
    object is used to dispatch topic events to a registered @ref EventSlot.

    Subscription handles are reference counting, meaning that every time a copy
    of a handle is made, the reference count increases. Every time a bound
    Subscription handle is destroyed, the reference count decreases. When the
    reference count reaches zero, the topic is automatically unsubscribed. This
    reference counting scheme is provided to help automate the management of
    topic subscriptions using RAII techniques.

    Here's a (contrived) example illustrating the reference counting nature of
    Subscription:

    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    boost::asio::io_service iosvc;
    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        auto client = CoroClient<>::create(connectorList);
        client->connect(yield);
        SessionId sid = client->join("somerealm", yield);
        {
            auto sub = client->subscribe<void>("topic", slot, yield);

            // The sub object gets destroyed as it leaves this scope.
            // Since there are no other Subscription handles sharing the same
            // underlying subscription object, the subscription will be
            // automatically unsubscribed.
        }
    });
    iosvc.run();
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Subscriptions can also be manually unsubscribed via
    Subscription::unsubscribe. */
//------------------------------------------------------------------------------
class Subscription
{
public:
    /// Asyncronous completion handler type used for unsubscribing
    using UnsubscribeHandler = AsyncHandler<bool>;

    /** Default constructor. */
    Subscription();

    /** Conversion to `bool` operator returning `true` if this handle is bound
        to a subscription. */
    explicit operator bool() const;

    /** Returns the topic URI associated with this subscription. */
    const std::string& topic() const;

    /** Returns the ID number of this subscription. */
    SubscriptionId id() const;

    /** Returns the number of Subscription handles managing the same
        subscription as this one does. */
    long useCount() const;

    /** Explicitly unsubscribes the topic. */
    void unsubscribe();

    /** Asynchronously unsubscribes the topic, waiting for an acknowledgement
        from the broker. */
    void unsubscribe(UnsubscribeHandler handler);

private:
    Subscription(std::shared_ptr<internal::SubscriptionBase> impl);

    std::shared_ptr<internal::SubscriptionBase> impl_;

    friend class Client;
    template <typename, typename> friend class internal::ClientImpl;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/subscription.ipp"
#endif

#endif // CPPWAMP_SUBSCRIPTION_HPP
