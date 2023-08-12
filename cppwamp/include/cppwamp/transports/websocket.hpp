/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WEBSOCKET_HPP
#define CPPWAMP_TRANSPORTS_WEBSOCKET_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing Websocket transports. */
//------------------------------------------------------------------------------

#include <memory>
#include <set>
#include <utility>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../connector.hpp"
#include "../listener.hpp"
#include "websocketendpoint.hpp"
#include "websockethost.hpp"

// TODO: Websocket over TLS

namespace wamp
{

// Forward declarations
namespace internal
{
struct WebsocketConnectorImpl;
struct WebsocketListenerImpl;
}

//------------------------------------------------------------------------------
/** Connector specialization that establishes a client-side Websocket transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Connector<Websocket> : public Connecting
{
public:
    /** Type containing the transport settings. */
    using Settings = WebsocketHost;

    /** Constructor. */
    Connector(IoStrand i, Settings s, int codecId);

    /** Destructor. */
    ~Connector() override;

    /** Starts establishing the transport connection, emitting a
        Transportable::Ptr via the given handler if successful. */
    void establish(Handler&& handler) override;

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
    std::unique_ptr<internal::WebsocketConnectorImpl> impl_;
};

//------------------------------------------------------------------------------
/** Listener specialization that establishes a server-side Websocket transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Websocket> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = WebsocketEndpoint;

    /** Collection type used for codec IDs. */
    using CodecIds = std::set<int>;

    /** Constructor. */
    Listener(IoStrand i, Settings s, CodecIds codecIds);

    /** Destructor. */
    ~Listener() override;

    void establish(Handler&& handler) override;

    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Listener(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    /// @}

private:
    std::unique_ptr<internal::WebsocketListenerImpl> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/websocket.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_WEBSOCKET_HPP
