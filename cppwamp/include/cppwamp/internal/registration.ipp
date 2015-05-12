/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "callee.hpp"
#include "config.hpp"

namespace wamp
{

/*******************************************************************************
 * Registration
*******************************************************************************/

/** @post `!!(*this) == false` */
CPPWAMP_INLINE Registration::Registration() {}

CPPWAMP_INLINE Registration::operator bool() const {return id_ != invalidId_;}

CPPWAMP_INLINE RegistrationId Registration::id() const {return id_;}

CPPWAMP_INLINE void Registration::unregister() const
{
    auto callee = callee_.lock();
    if (callee)
        callee->unregister(*this);
}

CPPWAMP_INLINE Registration::Registration(CalleePtr callee, RegistrationId id,
                                          internal::PassKey)
    : callee_(callee),
      id_(id)
{}


/*******************************************************************************
    ScopedRegistration
*******************************************************************************/

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration::ScopedRegistration() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE
ScopedRegistration::ScopedRegistration(ScopedRegistration&& other)
    : Base(std::move(other))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE
ScopedRegistration::ScopedRegistration(Registration registration)
    : Base(std::move(registration))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration::~ScopedRegistration()
{
    unregister();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration&
ScopedRegistration::operator=(ScopedRegistration&& other)
{
    unregister();
    Base::operator=(std::move(other));
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration&
ScopedRegistration::operator=(Registration subscription)
{
    unregister();
    Base::operator=(std::move(subscription));
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void ScopedRegistration::release()
{
    Base::operator=(Registration());
}
} // namespace wamp
