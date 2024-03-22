/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPTRAITS_HPP
#define CPPWAMP_INTERNAL_HTTPTRAITS_HPP

#include <tuple>
#include "../traits.hpp"
#include "../transports/httpprotocol.hpp"
#include "tcptraits.hpp"
#include "websockettraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct HttpTraits
{
    using WsTraits       = WebsocketTraits;
    using ServerSettings = HttpEndpoint;
    using Socket         = WsTraits::HttpSocket;
    using IsTls          = FalseType;
    using SslContextType = std::tuple<>;

    static ConnectionInfo makeConnectionInfo(const Socket& socket)
    {
        return TcpTraits::connectionInfo(socket, "HTTP");
    }

    static bool isSslTruncationError(boost::system::error_code ec)
    {
        return false;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPTRAITS_HPP
