/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPLISTENER_HPP
#define CPPWAMP_INTERNAL_HTTPLISTENER_HPP

#include <memory>
#include <set>
#include "tcplistener.hpp"
#include "httptransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
using HttpListenerConfig =
    BasicTcpListenerConfig<HttpServerTransport, HttpEndpoint>;

//------------------------------------------------------------------------------
using HttpListener = RawsockListener<HttpListenerConfig>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPLISTENER_HPP
