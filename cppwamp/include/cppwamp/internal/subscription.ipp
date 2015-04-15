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

//------------------------------------------------------------------------------
CPPWAMP_INLINE Subscription::~Subscription() {unsubscribe();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const Topic& Subscription::topic() const {return topic_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SubscriptionId Subscription::id() const {return id_;}

//------------------------------------------------------------------------------
/** @note Duplicate unsubscriptions are safely ignored. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Subscription::unsubscribe()
{
    auto sub = subscriber_.lock();
    if (sub)
        sub->unsubscribe(this);
}

//------------------------------------------------------------------------------
/** @details
    Equivalent to Session::unsubscribe(Subscription::Ptr, AsyncHandler<bool>).
    @note Duplicate unsubscriptions are safely ignored. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Subscription::unsubscribe(AsyncHandler<bool> handler)
{
    auto sub = subscriber_.lock();
    if (sub)
        sub->unsubscribe(this, std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Subscription::Subscription(SubscriberPtr subscriber,
                                          Topic&& topic)
    : subscriber_(subscriber),
      topic_(std::move(topic))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Subscription::setId(SubscriptionId id, internal::PassKey)
    {id_ = id;}

} // namespace wamp
