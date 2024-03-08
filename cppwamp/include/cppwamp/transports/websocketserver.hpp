/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WEBSOCKETSERVER_HPP
#define CPPWAMP_TRANSPORTS_WEBSOCKETSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing Websocket server transports. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "websocketprotocol.hpp"

// TODO: Websocket over TLS server

namespace wamp
{

// Forward declaration
namespace internal { class WebsocketListener; }

//------------------------------------------------------------------------------
/** Listener specialization that establishes a server-side Websocket transport.
    Users do not need to use this class directly and should instead pass
    wamp::WebsocketEndpoint to wamp::Router::openServer via
    wamp::ServerOptions. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Websocket> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = WebsocketEndpoint;

    /** Constructor. */
    Listener(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c,
             RouterLogger::Ptr l = {});

    ~Listener() override;

    void observe(Handler handler) override;

    void establish() override;

    ErrorOr<Transporting::Ptr> take() override;

    void drop() override;

    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Listener(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    /// @}

private:
    std::shared_ptr<internal::WebsocketListener> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/websocketserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETSERVER_HPP
