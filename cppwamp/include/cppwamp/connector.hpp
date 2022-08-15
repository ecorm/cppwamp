/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONNECTOR_HPP
#define CPPWAMP_CONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Connecting abstract base class. */
//------------------------------------------------------------------------------

#include <functional>
#include <memory>
#include <system_error>
#include <vector>
#include "api.hpp"
#include "asiodefs.hpp"

namespace wamp
{

// Forward declaration
namespace internal {class ClientInterface;}

//------------------------------------------------------------------------------
/** Interface for establishing client transport endpoints.
    A concrete Connecting instance is used to establish a transport connection
    from a client to a router. Once the connection is established, the connector
    creates a client implementation having the appropriate serializer and
    transport facilities.

    The Session class uses these Connecting objects when attempting to
    establish a connection to the router.
    @see connector */
//------------------------------------------------------------------------------
class CPPWAMP_API Connecting : public std::enable_shared_from_this<Connecting>
{
public:
    /// Shared pointer to a Connecting
    using Ptr = std::shared_ptr<Connecting>;

    /** Destructor. */
    virtual ~Connecting() {}

    virtual IoStrand strand() const = 0;

protected:
    /** Asynchronous handler function type called by Connecting::establish. */
    using Handler =
        std::function<void (std::error_code,
                            std::shared_ptr<internal::ClientInterface>)>;

    /** Creates a deep copy of this Connecting object. */
    virtual Connecting::Ptr clone() const = 0;

    /** Starts establishing a transport connection. */
    virtual void establish(Handler&& handler) = 0;

    /** Cancels a transport connection in progress.
        A TransportErrc::aborted error code will be returned via the
        Connecting::establish asynchronous handler. */
    virtual void cancel() = 0;

    friend class Session;
};


//------------------------------------------------------------------------------
/** List of Connecting objects to use when attempting connection. */
//------------------------------------------------------------------------------
using ConnectorList = std::vector<Connecting::Ptr>;

} // namespace wamp

#endif // CPPWAMP_CONNECTOR_HPP
