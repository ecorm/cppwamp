/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TLSTRAITS_HPP
#define CPPWAMP_INTERNAL_TLSTRAITS_HPP

#include <boost/asio/ssl/stream.hpp>
#include "../asiodefs.hpp"
#include "../traits.hpp"
#include "../transports/tlsprotocol.hpp"
#include "tcptraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct TlsTraits
{
    using NetProtocol      = boost::asio::ip::tcp;
    using UnderlyingSocket = NetProtocol::socket;
    using Socket           = boost::asio::ssl::stream<UnderlyingSocket>;
    using ClientSettings   = TlsHost;
    using ServerSettings   = TlsEndpoint;
    using IsTls            = TrueType;

    static ConnectionInfo connectionInfo(const Socket& socket)
    {
        return TcpTraits::connectionInfo(socket.next_layer(), "TLS");
    }

    static Timeout heartbeatInterval(const TlsHost& settings)
    {
        return settings.heartbeatInterval();
    }

    static Timeout heartbeatInterval(const TlsEndpoint&)
    {
        return unspecifiedTimeout;
    }

    static Socket makeClientSocket(IoStrand i, ClientSettings& s)
    {
        return Socket{std::move(i), s.sslContext({}).get()};
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TLSTRAITS_HPP
