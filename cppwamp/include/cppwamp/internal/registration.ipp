/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../registration.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

/*******************************************************************************
 * Registration
*******************************************************************************/

/** @post `bool(*this) == false` */
CPPWAMP_INLINE Registration::Registration() {}

CPPWAMP_INLINE Registration::operator bool() const {return !link_.expired();}

CPPWAMP_INLINE RegistrationId Registration::id() const {return regId_;}

/** The associated event slot is immediately disabled within the execution
    context where `unregister` is called. In multithreaded use, it's possible
    for the slot to be executed just after `unregister` is called if
    both are not serialized via a common execution strand. */
CPPWAMP_INLINE void Registration::unregister()
{
    auto link = link_.lock();
    if (link)
        link->remove();
}

CPPWAMP_INLINE Registration::Registration(internal::PassKey, Link::Ptr p)
    : link_(p),
      regId_(p->key())
{}

CPPWAMP_INLINE Registration::Key Registration::key(internal::PassKey) const
{
    auto link = link_.lock();
    return link ? link->key() : Key{};
}

CPPWAMP_INLINE void Registration::disarm(internal::PassKey)
{
    auto link = link_.lock();
    if (link)
        link->disarm();
}

CPPWAMP_INLINE bool Registration::canUnregister(
    internal::PassKey, const internal::ClientLike& owner) const
{
    auto link = link_.lock();
    return link ? link->canRemove(owner) : true;
}


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
    unregister();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration&
ScopedRegistration::operator=(ScopedRegistration&& other) noexcept
{
    unregister();
    Base::operator=(std::move(other));
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ScopedRegistration&
ScopedRegistration::operator=(Registration registration)
{
    unregister();
    Base::operator=(std::move(registration));
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void ScopedRegistration::release()
{
    Base::operator=(Registration());
}

} // namespace wamp
