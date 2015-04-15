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
/** Manages the lifetime of an RPC registration.

    Registration objects are returned by the `enroll` member functions of
    the _Session_ family of classes. These objects are used internally by
    Session to dispatch remote procedure calls to a registered _event slot_.

    Registration objects are returned via reference-counting shared pointers.
    When the reference count reaches zero, the procedure is automatically
    unregistered. This reference counting scheme is provided to help automate
    the lifetime management of RPC registrations using RAII techniques.

    Here's an example illustrating how shared pointers can be used
    to manage the lifetime of a registration:

    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    struct Delegate
    {
        void rpc(Invocation inv);

        Registration::Ptr reg;
    }

    int main()
    {
        boost::asio::io_service iosvc;
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(connectorList);
            session->connect(yield);
            session->join("somerealm", yield);

            {
                using std::placeholders;
                Delegate delegate;
                delegate.reg = session->enroll(
                                "procedure",
                                std::bind(&Delegate::rpc, delegate, _1),
                                yield);

            }  // When the 'delegate' object leaves this scope, the Registration
               // shared pointer reference count drops to zero. This will
               // automatically unregister the registration, thereby avoiding
               // further member function calls on the deleted 'delegate' object.
        });
        iosvc.run();
    }
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Registrations can also be manually unregistered via
    Registration::unregister.

    @see Session::enroll, CoroSession::enroll */
//------------------------------------------------------------------------------
class Registration
{
public:
    using Ptr     = std::shared_ptr<Registration>;
    using WeakPtr = std::weak_ptr<Registration>;

    /** Automatically unregisters the RPC. */
    virtual ~Registration();

    /** Obtains the procedure information associated with this registration. */
    const Procedure& procedure() const;

    /** Obtains the ID number of this registration. */
    RegistrationId id() const;

    /** Explicitly unregisters the RPC. */
    void unregister();

    /** Asynchronously unregisters the RPC, waiting for an acknowledgement
        from the dealer. */
    void unregister(AsyncHandler<bool> handler);

protected:
    using CalleePtr = std::weak_ptr<internal::Callee>;

    Registration(CalleePtr callee, Procedure&& procedure);

private:
    CalleePtr callee_;
    Procedure procedure_;
    RegistrationId id_ = -1;

public:
    // Internal use only
    virtual void invoke(Invocation&& invocation, internal::PassKey) = 0;
    void setId(RegistrationId id, internal::PassKey);

};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/registration.ipp"
#endif

#endif // CPPWAMP_REGISTRATION_HPP
