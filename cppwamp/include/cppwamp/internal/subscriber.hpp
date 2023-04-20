/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SUBSCRIBER_HPP
#define CPPWAMP_INTERNAL_SUBSCRIBER_HPP

#include <memory>
#include <string>
#include "../subscription.hpp"

namespace wamp
{

class Error;
class Subscription;

namespace internal
{

//------------------------------------------------------------------------------
class Subscriber
{
public:
    using WeakPtr = std::weak_ptr<Subscriber>;

    virtual ~Subscriber() {}

    virtual void unsubscribe(const Subscription&) = 0;

    virtual void safeUnsubscribe(const Subscription&) = 0;

    virtual void onEventError(Error&&, SubscriptionId) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SUBSCRIBER_HPP
