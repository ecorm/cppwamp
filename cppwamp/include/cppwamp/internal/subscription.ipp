/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "subscriptionimpl.hpp"
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** @post `!!*this == false`
    @post `this->useCount() == 0` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Subscription::Subscription() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Subscription::operator bool() const
{
    return !!impl_;
}

//------------------------------------------------------------------------------
/** @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Subscription::topic() const
{
    return impl_->topic();
}

//------------------------------------------------------------------------------
/** @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE SubscriptionId Subscription::id() const
{
    return impl_->id();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE long Subscription::useCount() const
{
    return impl_.use_count();
}

//------------------------------------------------------------------------------
/** @note Duplicate unsubscriptions are safely ignored.
    @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Subscription::unsubscribe()
{
    impl_->unsubscribe();
}

//------------------------------------------------------------------------------
/** @details
    Equivalent to Client::unsubscribe(Subscription, AsyncHandler<bool>).
    @note Duplicate unsubscriptions are safely ignored.
    @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Subscription::unsubscribe(UnsubscribeHandler handler)
{
    impl_->unsubscribe(std::move(handler));
}

#ifndef CPPWAMP_FOR_DOXYGEN
//------------------------------------------------------------------------------
CPPWAMP_INLINE Subscription::Subscription(internal::SubscriptionBase::Ptr impl)
    : impl_(std::move(impl)) {}
#endif

} // namespace wamp
