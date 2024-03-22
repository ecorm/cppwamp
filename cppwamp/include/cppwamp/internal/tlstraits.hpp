/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TLSTRAITS_HPP
#define CPPWAMP_INTERNAL_TLSTRAITS_HPP

#include <boost/asio/ssl/error.hpp>
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
    using SslContextType   = SslContext;

    static ConnectionInfo connectionInfo(const Socket& socket,
                                         const char* protocol = "TLS")
    {
        return TcpTraits::connectionInfo(socket.next_layer(), protocol);
    }

    static Timeout heartbeatInterval(const TlsHost& settings)
    {
        return settings.heartbeatInterval();
    }

    static Timeout heartbeatInterval(const TlsEndpoint&)
    {
        return unspecifiedTimeout;
    }

    static ErrorOr<SslContextType> makeClientSslContext(const ClientSettings& s)
    {
        return s.makeSslContext({});
    }

    static Socket makeClientSocket(IoStrand i, SslContextType& c)
    {
        return Socket{std::move(i), c.get()};
    }

    template <typename TSslSocket, typename TSettings>
    static std::error_code initializeClientSocket(TSslSocket& socket,
                                                  const TSettings& settings)
    {
        struct Verified
        {
            SslVerifyOptions::VerifyCallback callback;

            bool operator()(bool preverified,
                            boost::asio::ssl::verify_context& ctx) const
            {
                return callback(preverified,
                                SslVerifyContext{ctx.native_handle()});
            }
        };

        const auto& vo = settings.sslVerifyOptions();

        boost::system::error_code ec;

        if (vo.modeIsSpecified())
            socket.set_verify_mode(vo.mode(), ec);

        if (!ec && vo.depth() != 0)
            socket.set_verify_depth(vo.depth(), ec);

        if (!ec && vo.callback() != nullptr)
            socket.set_verify_callback(Verified{vo.callback()}, ec);

        return ec;
    }

    static bool isSslTruncationError(boost::system::error_code ec)
    {
        return ec == boost::asio::ssl::error::stream_truncated;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TLSTRAITS_HPP
