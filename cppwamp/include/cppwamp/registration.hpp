/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_REGISTRATION_HPP
#define CPPWAMP_REGISTRATION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Registration class. */
//------------------------------------------------------------------------------

#include "api.hpp"
#include "wampdefs.hpp"
#include "internal/passkey.hpp"
#include "internal/slotlink.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Represents a remote procedure registration.

    A Registration is a lightweight object returned by Session::enroll. This
    objects allows users to unregister the RPC registration.

    It is always safe to unregister via a Registration object. If the Session
    or the registration no longer exists, an unregister operation effectively
    does nothing. Duplicate unregisters are safely ignored.

    @see ScopedRegistration */
//------------------------------------------------------------------------------
class CPPWAMP_API Registration
{
public:
    /** Constructs an empty registration. */
    Registration();

    /** Returns true if the registration is still active. */
    explicit operator bool() const;

    /** Obtains the ID number of this registration. */
    RegistrationId id() const;

    /** Unregisters the RPC. */
    void unregister();

private:
    using Link = internal::RegistrationLink;
    using Key = internal::RegistrationLink::Key;

    Link::WeakPtr link_;
    RegistrationId regId_;

public:
    // Internal use only
    Registration(internal::PassKey, Link::Ptr p);
    Key key(internal::PassKey) const;
    void disarm(internal::PassKey);
    bool canUnregister(internal::PassKey,
                       const internal::ClientLike& owner) const;
};


//------------------------------------------------------------------------------
/** Limits a Registration's lifetime to a particular scope.

    @see @ref ScopedRegistrations
    @see Registration, Session::enroll */
//------------------------------------------------------------------------------
class CPPWAMP_API ScopedRegistration : public Registration
{
// This class is modeled after boost::signals2::scoped_connection.
public:
    /** Default constructs an empty ScopedRegistration. */
    ScopedRegistration();

    /** Move constructor. */
    ScopedRegistration(ScopedRegistration&& other) noexcept;

    /** Converting constructor taking a Registration object to manage. */
    ScopedRegistration(Registration registration);

    /** Destructor which automatically unsubscribes the subscription. */
    ~ScopedRegistration();

    /** Move assignment. */
    ScopedRegistration& operator=(ScopedRegistration&& other) noexcept;

    /** Assigns another Regisration to manage.
        The old registration is automatically unregistered. */
    ScopedRegistration& operator=(Registration registration);

    /** Releases the registration so that it will no longer be automatically
        unregistered if the ScopedRegistration is destroyed or reassigned. */
    void release();

    /** Non-copyable. */
    ScopedRegistration(const ScopedRegistration&) = delete;

    /** Non-copyable. */
    ScopedRegistration& operator=(const ScopedRegistration&) = delete;

private:
    using Base = Registration;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/registration.ipp"
#endif

#endif // CPPWAMP_REGISTRATION_HPP
