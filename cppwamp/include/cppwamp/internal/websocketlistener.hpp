/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP

#include <memory>
#include <set>
#include "tcplistener.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
using WebsocketListenerConfig =
    BasicTcpListenerConfig<WebsocketServerTransport, WebsocketEndpoint>;

//------------------------------------------------------------------------------
class WebsocketListener : public RawsockListener<WebsocketListenerConfig>
{
public:
    using Ptr = std::shared_ptr<WebsocketListener>;
    using RawsockListener<WebsocketListenerConfig>::RawsockListener;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
