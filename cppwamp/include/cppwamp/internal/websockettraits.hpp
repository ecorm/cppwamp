/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETTRAITS_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETTRAITS_HPP

#include <tuple>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket/stream.hpp>
#include "../traits.hpp"
#include "../transports/websocketprotocol.hpp"
#include "tcptraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct WebsocketTraits
{
    using ClientSettings = WebsocketHost;
    using ServerSettings = WebsocketEndpoint;
    using IsTls          = FalseType;
    using SslContextType = std::tuple<>;
    using TcpSocket      = boost::asio::ip::tcp::socket;
    using HttpSocket     = TcpSocket;
    using Socket         = boost::beast::websocket::stream<HttpSocket>;

    static ConnectionInfo makeConnectionInfo(const HttpSocket& s)
    {
        return TcpTraits::connectionInfo(s, "WS");
    }

    static ConnectionInfo makeConnectionInfo(const Socket& s)
    {
        return makeConnectionInfo(tcpLayer(s));
    }

    static TcpSocket& tcpLayer(Socket& s) {return s.next_layer();}

    static const TcpSocket& tcpLayer(const Socket& s) {return s.next_layer();}

    static bool isSslTruncationError(boost::system::error_code) {return false;}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRAITS_HPP
