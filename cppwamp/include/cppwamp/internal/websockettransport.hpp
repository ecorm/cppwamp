/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP

#include "basicwebsockettransport.hpp"
#include "websockettraits.hpp"

namespace wamp
{

namespace internal
{

using WebsocketClientTransport = BasicWebsocketClientTransport<WebsocketTraits>;

using WebsocketServerTransport = BasicWebsocketServerTransport<WebsocketTraits>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
