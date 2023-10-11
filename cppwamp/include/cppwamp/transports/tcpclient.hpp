/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TCPCLIENT_HPP
#define CPPWAMP_TRANSPORTS_TCPCLIENT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing TCP client transports. */
//------------------------------------------------------------------------------

#include <memory>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../connector.hpp"
#include "tcpprotocol.hpp"

// TODO: TLS Client Transport

namespace wamp
{

// Forward declaration
namespace internal { class TcpConnector; }

//------------------------------------------------------------------------------
/** Connector specialization that establishes a client-side TCP transport.
    Users should not use this class directly, but rather pass a
    wamp::ConnectionWish instead to wamp::Session::connect. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Connector<Tcp> : public Connecting
{
public:
    /** Type containing the transport settings. */
    using Settings = TcpHost;

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
    std::shared_ptr<internal::TcpConnector> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tcpclient.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TCPCLIENT_HPP
