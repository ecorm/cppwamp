/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic Websocket protocol facilities. */
//------------------------------------------------------------------------------

#include <string>
#include <system_error>
#include "../api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the Websocket transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Websocket
{
    constexpr Websocket() = default;
};

//------------------------------------------------------------------------------
/** %Error code values used with the WebsocketCloseCategory error category. */
//------------------------------------------------------------------------------
enum class WebsocketCloseErrc
{
    unknown        =    1, /// Websocket connection closed abnormally for unknown reason
    normal         = 1000, /// Websocket connection successfully fulfilled its purpose
    goingAway      = 1001, /// Websocket peer is navigating away or going down
    protocolError  = 1002, /// Websocket protocol error
    unknownData    = 1003, /// Websocket peer cannot accept data type
    badPayload     = 1007, /// Invalid websocket message data type
    policyError    = 1008, /// Websocket peer received a message violating its policy
    tooBig         = 1009, /// Websocket peer received a message too big to process
    needsExtension = 1010, /// Websocket server lacks extension expected by client
    internalError  = 1011, /// Websocket server encountered an unexpected condition
    serviceRestart = 1012, /// Websocket server is restarting
    tryAgainLater  = 1013, /// Websocket connection terminated due to temporary server condition
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting Websocket close reasons.
    @see WebsocketCloseErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API WebsocketCloseCategory : public std::error_category
{
public:
    /** Obtains the name of the category. */
    const char* name() const noexcept override;

    /** Obtains the explanatory string. */
    std::string message(int ev) const override;

    /** Compares `error_code` and and error condition for equivalence. */
    bool equivalent(const std::error_code& code,
                    int condition) const noexcept override;

private:
    CPPWAMP_HIDDEN WebsocketCloseCategory();

    friend WebsocketCloseCategory& websocketCloseCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Websocket
    close reasons.
    @relates WebsocketCloseCategory */
//------------------------------------------------------------------------------
CPPWAMP_API WebsocketCloseCategory& websocketCloseCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a WebsocketCloseErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(WebsocketCloseErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a WebsocketCloseErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(WebsocketCloseErrc errc);


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/websocketprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP
