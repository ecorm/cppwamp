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
    using SslContextType   = SslContext;

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

    static SslContextType makeClientSslContext(const ClientSettings& s)
    {
        return s.makeSslContext({});
    }

    static Socket makeClientSocket(IoStrand i, const ClientSettings& s,
                                   SslContextType& c)
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

        Socket sslStream{std::move(i), c.get()};
        const auto& vo = s.sslVerifyOptions();

        if (vo.modeIsSpecified())
            sslStream.set_verify_mode(vo.mode());

        if (vo.depth() != 0)
            sslStream.set_verify_depth(vo.depth());

        if (vo.callback() != nullptr)
            sslStream.set_verify_callback(Verified{vo.callback()});

        return sslStream;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TLSTRAITS_HPP
