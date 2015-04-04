/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "config.hpp"
#include "registrationimpl.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** @post `!!*this == false`
    @post `this->useCount() == 0` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Registration::Registration() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Registration::operator bool() const
{
    return !!impl_;
}

//------------------------------------------------------------------------------
/** @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Registration::procedure() const
{
    return impl_->procedure();
}

//------------------------------------------------------------------------------
/** @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE RegistrationId Registration::id() const
{
    return impl_->id();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE long Registration::useCount() const
{
    return impl_.use_count();
}

//------------------------------------------------------------------------------
/** @note Duplicate unregistrations are safely ignored.
    @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Registration::unregister()
{
    impl_->unregister();
}

//------------------------------------------------------------------------------
/** @details
    Equivalent to Client::unregister(Registration, AsyncHandler<bool>).
    @note Duplicate unregistrations are safely ignored.
    @pre `!!*this == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Registration::unregister(UnregisterHandler handler)
{
    impl_->unregister(handler);
}

#ifndef CPPWAMP_FOR_DOXYGEN
//------------------------------------------------------------------------------
CPPWAMP_INLINE Registration::Registration(
        std::shared_ptr<wamp::internal::RegistrationBase> impl)
    : impl_(std::move(impl))
{}
#endif

} // namespace wamp
