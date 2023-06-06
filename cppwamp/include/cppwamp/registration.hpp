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

#include <memory>
#include <string>
#include "api.hpp"
#include "wampdefs.hpp"
#include "internal/clientcontext.hpp"
#include "internal/passkey.hpp"

// TODO: Fix race between unregistration and handler execution

namespace wamp
{

//------------------------------------------------------------------------------
/** Represents a remote procedure registration.

    A Registration is a lightweight object returned by Session::enroll. This
    objects allows users to unregister the RPC registration.

    It is always safe to unregister via a Registration object. If the Session
    or the registration no longer exists, an unregister operation effectively
    does nothing.

    @see ScopedRegistration */
//------------------------------------------------------------------------------
class CPPWAMP_API Registration
{
public:
    /** Constructs an empty registration. */
    Registration();

    /** Copy constructor. */
    Registration(const Registration& other);

    /** Move constructor. */
    Registration(Registration&& other) noexcept;

    /** Returns false if the registration is empty. */
    explicit operator bool() const;

    /** Obtains the ID number of this registration. */
    RegistrationId id() const;

    /** Copy assignment. */
    Registration& operator=(const Registration& other);

    /** Move assignment. */
    Registration& operator=(Registration&& other) noexcept;

    /** Unregisters the RPC. */
    void unregister();

private:
    internal::ClientContext callee_;
    RegistrationId id_ = nullId();

public:
    // Internal use only
    Registration(internal::PassKey, internal::ClientContext callee,
                 RegistrationId id);
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
