/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_REGISTRATION_HPP
#define CPPWAMP_REGISTRATION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the Registration class. */
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
namespace internal { class Callee; }

//------------------------------------------------------------------------------
/** Represents a remote procedure registration.

    A Registration is a lightweight object returned by the `enroll` member
    functions of the _Session_ family of classes. This objects allows users to
    unregister the RPC registration.

    It is always safe to unregister via a Registration object. If the Session
    or the registration no longer exists, an unregister operation effectively
    does nothing.

    @see ScopedRegistration, Session::enroll, CoroSession::enroll */
//------------------------------------------------------------------------------
class Registration
{
public:
    /** Constructs an empty registration. */
    Registration();

    /** Returns false if the registration is empty. */
    explicit operator bool() const;

    /** Obtains the ID number of this registration. */
    RegistrationId id() const;

    /** Unregisters the RPC. */
    void unregister() const;

public:
    // Internal use only
    using CalleePtr = std::weak_ptr<internal::Callee>;
    Registration(CalleePtr callee, RegistrationId id, internal::PassKey);

private:
    static constexpr RegistrationId invalidId_ = -1;

    CalleePtr callee_;
    RegistrationId id_ = invalidId_;
};


//------------------------------------------------------------------------------
/** Limits a Registration's lifetime to a particular scope.

    @see @ref ScopedRegistrations
    @see Registration, Session::enroll, CoroSession::enroll */
//------------------------------------------------------------------------------
class ScopedRegistration : public Registration
{
// This class is modeled after boost::signals2::scoped_connection.
public:
    /** Default constructs an empty ScopedRegistration. */
    ScopedRegistration();

    /** Move constructor. */
    ScopedRegistration(ScopedRegistration&& other);

    /** Converting constructor taking a Registration object to manage. */
    ScopedRegistration(Registration registration);

    /** Destructor which automatically unsubscribes the subscription. */
    ~ScopedRegistration();

    /** Move assignment. */
    ScopedRegistration& operator=(ScopedRegistration&& other);

    /** Assigns another Regisration to manage.
        The old registration is automatically unregistered. */
    ScopedRegistration& operator=(Registration subscription);

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
