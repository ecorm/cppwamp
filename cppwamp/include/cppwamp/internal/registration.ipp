/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../registration.hpp"
#include <utility>
#include "callee.hpp"
#include "../api.hpp"

namespace wamp
{

/*******************************************************************************
 * Registration
*******************************************************************************/

/** @post `!!(*this) == false` */
CPPWAMP_INLINE Registration::Registration() {}

CPPWAMP_INLINE Registration::Registration(const Registration& other)
    : callee_(other.callee_),
      id_(other.id_)
{}

/** @post `!other == true` */
CPPWAMP_INLINE Registration::Registration(Registration&& other) noexcept
    : callee_(other.callee_),
      id_(other.id_)
{
    other.callee_.reset();
    other.id_ = invalidId_;
}

CPPWAMP_INLINE Registration::operator bool() const {return id_ != invalidId_;}

CPPWAMP_INLINE RegistrationId Registration::id() const {return id_;}

CPPWAMP_INLINE Registration& Registration::operator=(const Registration& other)
{
    callee_ = other.callee_;
    id_ = other.id_;
    return *this;
}

/** @post `!other == true` */
CPPWAMP_INLINE Registration&
Registration::operator=(Registration&& other) noexcept
{
    callee_ = other.callee_;
    id_ = other.id_;
    other.callee_.reset();
    other.id_ = invalidId_;
    return *this;
}

CPPWAMP_INLINE void Registration::unregister() const
{
    auto callee = callee_.lock();
    if (callee)
        callee->unregister(*this);
}

CPPWAMP_INLINE void Registration::unregister(ThreadSafe) const
{
    auto callee = callee_.lock();
    if (callee)
        callee->safeUnregister(*this);
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
ScopedRegistration::ScopedRegistration(ScopedRegistration&& other) noexcept
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
    unregister(threadSafe);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration&
ScopedRegistration::operator=(ScopedRegistration&& other) noexcept
{
    unregister(threadSafe);
    Base::operator=(std::move(other));
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration&
ScopedRegistration::operator=(Registration subscription)
{
    unregister(threadSafe);
    Base::operator=(std::move(subscription));
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void ScopedRegistration::release()
{
    Base::operator=(Registration());
}
} // namespace wamp
