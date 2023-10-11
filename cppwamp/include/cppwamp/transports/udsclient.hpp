/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_UDSCLIENT_HPP
#define CPPWAMP_TRANSPORTS_UDSCLIENT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing Unix domain socket client
           transports. */
//------------------------------------------------------------------------------

#include <memory>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../connector.hpp"
#include "udsprotocol.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class UdsConnector; }


//------------------------------------------------------------------------------
/** Connector specialization that establishes a Unix domain socket transport.
    Users do not need to use this class directly and should pass
    wamp::ConnectionWish instead to wamp::Session::connect. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Connector<Uds> : public Connecting
{
public:
    /** Type containing the transport settings. */
    using Settings = UdsHost;

    /** Constructor. */
    Connector(IoStrand i, Settings s, int codecId);

    /** Destructor. */
    ~Connector() override;

    /** Starts establishing the transport connection, emitting a
        Transportable::Ptr via the given handler if successful. */
    void establish(Handler handler) override;

    /** Cancels transport connection in progress, emitting an error code
        via the handler passed to the establish method. */
    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Connector(const Connector&) = delete;
    Connector(Connector&&) = delete;
    Connector& operator=(const Connector&) = delete;
    Connector& operator=(Connector&&) = delete;
    /// @}

private:
    std::shared_ptr<internal::UdsConnector> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/udsclient.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_UDSCLIENT_HPP
