/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPSTRAITS_HPP
#define CPPWAMP_INTERNAL_HTTPSTRAITS_HPP

#include "../traits.hpp"
#include "../transports/httpsprotocol.hpp"
#include "tcptraits.hpp"
#include "tlstraits.hpp"
#include "wsstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct HttpsTraits
{
    using WsTraits       = WssTraits;
    using ServerSettings = HttpsEndpoint;
    using Socket         = WsTraits::HttpSocket;
    using IsTls          = TrueType;
    using SslContextType = SslContext;

    static ConnectionInfo makeConnectionInfo(const Socket& socket)
    {
        return TcpTraits::connectionInfo(socket.next_layer(), "HTTPS");
    }

    static bool isSslTruncationError(boost::system::error_code ec)
    {
        return ec == boost::asio::ssl::error::stream_truncated;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPSTRAITS_HPP
