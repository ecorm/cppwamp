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

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP
