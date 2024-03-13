/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WSSTRANSPORT_HPP
#define CPPWAMP_INTERNAL_WSSTRANSPORT_HPP

#include "basicwebsockettransport.hpp"
#include "wsstraits.hpp"

namespace wamp
{

namespace internal
{

using WssStream = BasicWebsocketStream<WssTraits>;

using WssAdmitter = BasicWebsocketAdmitter<WssTraits>;

using WssClientTransport = BasicWebsocketClientTransport<WssTraits>;

using WssServerTransport = BasicWebsocketServerTransport<WssTraits>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WSSTRANSPORT_HPP
