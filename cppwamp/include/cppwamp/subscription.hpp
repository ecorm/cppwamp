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
    Contains the declaration of the Subscription class. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "asyncresult.hpp"
#include "sessiondata.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class Subscriber; }

//------------------------------------------------------------------------------
/** Manages the lifetime of a pub/sub event subscription.

    Subscription objects are returned by the `subscribe` member functions of
    the _Session_ family of classes. These objects are used internally by
    Session to dispatch pub/sub events to a registered _event slot_.

    Subscription objects are returned via reference-counting shared pointers.
    When the reference count reaches zero, the topic is automatically
    unsubscribed. This reference counting scheme is provided to help automate
    the lifetime management of topic subscriptions using RAII techniques.

    Here's an example illustrating how shared pointers can be used
    to manage the lifetime of a Subscription:

    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    struct Observer
    {
        void onEvent(Event event);

        Subscription::Ptr sub;
    }

    int main()
    {
        boost::asio::io_service iosvc;
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(connectorList);
            session->connect(yield);
            session->join("somerealm", yield);

            {
                using std::placeholders;
                Observer observer;
                observer.sub = session->subscribe(
                                "topic",
                                std::bind(&Observer::onEvent, observer, _1),
                                yield);

            }  // When the 'observer' object leaves this scope, the Subscription
               // shared pointer reference count drops to zero. This will
               // automatically unsubscribe the subscription, thereby avoiding
               // further member function calls on the deleted 'observer' object.
        });
        iosvc.run();
    }
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Subscriptions can also be manually unsubscribed via
    Subscription::unsubscribe.

    @see Session::subscribe, CoroSession::subscribe */
//------------------------------------------------------------------------------
class Subscription
{
public:
    using Ptr     = std::shared_ptr<Subscription>;
    using WeakPtr = std::weak_ptr<Subscription>;

    /** Automatically unsubscribes from the topic. */
    virtual ~Subscription();

    /** Obtains the topic information associated with this subscription. */
    const Topic& topic() const;

    /** Obtains the ID number of this subscription. */
    SubscriptionId id() const;

    /** Explicitly unsubscribes from the topic. */
    void unsubscribe();

    /** Asynchronously unsubscribes from the topic, waiting for an
        acknowledgement from the broker. */
    void unsubscribe(AsyncHandler<bool> handler);

protected:
    using SubscriberPtr = std::weak_ptr<internal::Subscriber>;

    Subscription(SubscriberPtr subscriber, Topic&& topic);

private:
    SubscriberPtr subscriber_;
    Topic topic_;
    SubscriptionId id_ = -1;

public:
    // Internal use only
    virtual void invoke(Event&& event, internal::PassKey) = 0;
    void setId(SubscriptionId id, internal::PassKey);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/subscription.ipp"
#endif

#endif // CPPWAMP_SUBSCRIPTION_HPP
