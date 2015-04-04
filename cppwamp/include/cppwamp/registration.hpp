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
    Contains the declaration of the Registration handle. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "asyncresult.hpp"
#include "wampdefs.hpp"

namespace wamp
{

// Forward declarations
namespace internal
{
    template <typename, typename> class ClientImpl;
    class RegistrationBase;
}

//------------------------------------------------------------------------------
/** Reference-counting handle used to manage the lifetime of an RPC
    registration.
    Registration handles are returned by the `enroll` member function of the
    client family of classes (see Client::enroll, CoroClient::enroll,
    CoroErrcClient::enroll). These handles point to an underlying RPC
    registration object managed by a client. This underlying registration
    object is used to dispatch RPC invocations to a registered @ref CallSlot.

    Registration handles are reference counting, meaning that every time a copy
    of a handle is made, the reference count increases. Every time a bound
    Registration handle is destroyed, the reference count decreases. When the
    reference count reaches zero, the RPC is automatically unregistered. This
    reference counting scheme is provided to help automate the management of
    RPC registrations using RAII techniques.

    Here's a (contrived) example illustrating the reference counting nature of
    Registration:

    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    boost::asio::io_service iosvc;
    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        auto client = CoroClient<>::create(connectorList);
        client->connect(yield);
        SessionId sid = client->join("somerealm", yield);
        {
            auto reg = client->enroll<void>("procedure", slot, yield);

            // The reg object gets destroyed as it leaves this scope.
            // Since there are no other Registration handles sharing the same
            // underlying registration object, the registration will be
            // automatically unregistered.
        }
    });
    iosvc.run();
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Registrations can also be manually unregistered via
    Registration::unregister. */
//------------------------------------------------------------------------------
class Registration
{
public:
    /// Asyncronous completion handler type used for unregistering
    using UnregisterHandler = AsyncHandler<bool>;

    /** Default constructor. */
    Registration();

    /** Conversion to `bool` operator returning `true` if this handle is bound
        to a registration. */
    explicit operator bool() const;

    /** Returns the procedure URI associated with this registration. */
    const std::string& procedure() const;

    /** Returns the ID number of this registration. */
    RegistrationId id() const;

    /** Returns the number of Registration handles managing the same
        registration as this one does. */
    long useCount() const;

    /** Explicitly unregisters the RPC. */
    void unregister();

    /** Asynchronously unregisters the RPC, waiting for an acknowledgement
        from the dealer. */
    void unregister(UnregisterHandler handler);

private:
    Registration(std::shared_ptr<internal::RegistrationBase> impl);

    std::shared_ptr<internal::RegistrationBase> impl_;

    template <typename, typename> friend class internal::ClientImpl;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/registration.ipp"
#endif

#endif // CPPWAMP_REGISTRATION_HPP
