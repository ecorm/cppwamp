/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WSSTRAITS_HPP
#define CPPWAMP_INTERNAL_WSSTRAITS_HPP

#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include "../traits.hpp"
#include "../transports/wssprotocol.hpp"
#include "tlstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct WssTraits
{
    using ClientSettings = WssHost;
    using ServerSettings = WssEndpoint;
    using IsTls          = TrueType;
    using SslContextType = SslContext;
    using TcpSocket      = boost::asio::ip::tcp::socket;
    using HttpSocket     = boost::beast::ssl_stream<TcpSocket>;
    using Socket         = boost::beast::websocket::stream<HttpSocket>;

    static ConnectionInfo makeConnectionInfo(const HttpSocket& s)
    {
        return TcpTraits::connectionInfo(s.next_layer(), "WSS");
    }

    static ConnectionInfo makeConnectionInfo(const Socket& s)
    {
        return makeConnectionInfo(s.next_layer());
    }

    static TcpSocket& tcpLayer(Socket& s) {return s.next_layer().next_layer();}

    static const TcpSocket& tcpLayer(const Socket& s)
    {
        return s.next_layer().next_layer();
    }

    static TcpSocket& tcpLayer(HttpSocket& s) {return s.next_layer();}

    static const TcpSocket& tcpLayer(const HttpSocket& s)
    {
        return s.next_layer();
    }

    static bool isSslTruncationError(boost::system::error_code ec)
    {
        return ec == boost::asio::ssl::error::stream_truncated;
    }

    static std::error_code initializeClientSocket(
        Socket& socket, const ClientSettings& settings)
    {
        return TlsTraits::initializeClientSocket(socket.next_layer(), settings);
    }

    template <typename F>
    static void sslClientHandshake(Socket& s, F&& handler)
    {
        s.next_layer().async_handshake(
            boost::asio::ssl::stream_base::client, std::forward<F>(handler));
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WSSTRAITS_HPP
