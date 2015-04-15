/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONNECTOR_HPP
#define CPPWAMP_CONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the Connector abstract base class. */
//------------------------------------------------------------------------------

#include <functional>
#include <memory>
#include <vector>

namespace wamp
{

// Forward declaration
namespace internal {class ClientInterface;}

//------------------------------------------------------------------------------
/** Abstract base class for establishing client transport endpoints.
    A Connector is used to establish a transport connection from a client
    to a router. Once the connection is established, Connector creates a client
    implementation having the appropriate serializer and transport
    facilities.

    The Session class uses these Connector objects when attempting to
    establish a connection to the router.
    @see TcpConnector
    @see UdsConnector
    @see legacy::TcpConnector
    @see legacy::UdsConnector */
//------------------------------------------------------------------------------
class Connector : public std::enable_shared_from_this<Connector>
{
public:
    /// Shared pointer to a Connector
    using Ptr = std::shared_ptr<Connector>;

    /** Destructor. */
    virtual ~Connector() {}

protected:
    /** Asynchronous handler function type called by Connector::establish. */
    using Handler =
        std::function<void (std::error_code,
                            std::shared_ptr<internal::ClientInterface>)>;

    /** Creates a deep copy of this Connector object. */
    virtual Connector::Ptr clone() const = 0;

    /** Starts establishing a transport connection. */
    virtual void establish(Handler handler) = 0;

    /** Cancels a transport connection is progress.
        A TransportErrc::aborted error code will be returned via the
        Connector::establish asynchronous handler. */
    virtual void cancel() = 0;

    friend class Session;
};


//------------------------------------------------------------------------------
/** List of Connector objects to use when attempting connection. */
//------------------------------------------------------------------------------
using ConnectorList = std::vector<Connector::Ptr>;

} // namespace wamp

#endif // CPPWAMP_CONNECTOR_HPP
