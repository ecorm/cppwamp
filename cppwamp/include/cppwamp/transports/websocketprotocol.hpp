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

#include <array>
#include <string>
#include "../api.hpp"
#include "../codec.hpp"

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
inline const std::string& websocketSubprotocolString(int codecId)
{
    static const std::array<std::string, KnownCodecIds::count() + 1> ids =
    {
        "",
        "wamp.2.json",
        "wamp.2.msgpack",
        "wamp.2.cbor"
    };

    if (codecId > KnownCodecIds::count())
        return ids[0];
    return ids.at(codecId);
}

//------------------------------------------------------------------------------
inline bool websocketSubprotocolIsText(int codecId)
{
    return codecId == KnownCodecIds::json();
}

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP
