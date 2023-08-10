/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocketprotocol.hpp"
#include <array>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE const char* WebsocketCloseCategory::name() const noexcept
{
    return "wamp::WebsocketCloseCategory";
}

CPPWAMP_INLINE std::string WebsocketCloseCategory::message(int ev) const
{
    static const std::array<const char*, 14> msg{
    {
/* normal         = 1000 */ "Websocket connection successfully fulfilled its purpose",
/* goingAway      = 1001 */ "Websocket peer is navigating away or going down",
/* protocolError  = 1002 */ "Websocket protocol error",
/* unknownData    = 1003 */ "Websocket peer cannot accept data type",
/*                  1004 */ "",
/*                  1005 */ "",
/*                  1006 */ "",
/* badPayload     = 1007 */ "Invalid websocket message data type",
/* policyError    = 1008 */ "Websocket peer received a message violating its policy",
/* tooBig         = 1009 */ "Websocket peer received a message too big to process",
/* needsExtension = 1010 */ "Websocket server lacks extension expected by client",
/* internalError  = 1011 */ "Websocket server encountered an unexpected condition",
/* serviceRestart = 1012 */ "Websocket server is restarting",
/* tryAgainLater  = 1013 */ "Websocket connection terminated due to temporary server condition"
    }};

    if (ev == 1)
        return "Websocket connection closed abnormally for unknown reason";

    if (ev < 1000 || ev > 1013)
        return {};
    return msg.at(ev - 1000);
}

CPPWAMP_INLINE bool WebsocketCloseCategory::equivalent(
    const std::error_code& code, int condition) const noexcept
{
    return (code.category() == websocketCloseCategory()) &&
           (code.value() == condition);
}

CPPWAMP_INLINE WebsocketCloseCategory::WebsocketCloseCategory() = default;

CPPWAMP_INLINE WebsocketCloseCategory& websocketCloseCategory()
{
    static WebsocketCloseCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(WebsocketCloseErrc errc)
{
    return {static_cast<int>(errc), websocketCloseCategory()};
}

CPPWAMP_INLINE std::error_condition
make_error_condition(WebsocketCloseErrc errc)
{
    return {static_cast<int>(errc), websocketCloseCategory()};
}

} // namespace wamp
