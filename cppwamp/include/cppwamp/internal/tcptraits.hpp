/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPTRAITS_HPP
#define CPPWAMP_INTERNAL_TCPTRAITS_HPP

#include <sstream>
#include <tuple>
#include <boost/asio/ip/tcp.hpp>
#include "../connectioninfo.hpp"
#include "../timeout.hpp"
#include "../traits.hpp"
#include "../transports/tcpprotocol.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct TcpTraits
{
    using NetProtocol      = boost::asio::ip::tcp;
    using UnderlyingSocket = NetProtocol::socket;
    using Socket           = UnderlyingSocket;
    using ClientSettings   = TcpHost;
    using ServerSettings   = TcpEndpoint;
    using IsTls            = FalseType;
    using SslContextType   = std::tuple<>;

    static ConnectionInfo connectionInfo(const NetProtocol::socket& socket,
                                         const char* protocol = "TCP")
    {
        static constexpr unsigned ipv4VersionNo = 4;
        static constexpr unsigned ipv6VersionNo = 6;

        boost::system::error_code ec;
        auto ep = socket.remote_endpoint(ec);
        if (ec)
            ep = {};

        std::ostringstream oss;
        if (ec)
            oss << "Error " << ec;
        else
            oss << ep;
        const auto addr = ep.address();
        const bool isIpv6 = addr.is_v6();

        Object details
        {
            {"address", addr.to_string()},
            {"ip_version", isIpv6 ? ipv6VersionNo : ipv4VersionNo},
            {"endpoint", oss.str()},
            {"port", ep.port()},
            {"protocol", protocol},
        };

        if (!isIpv6)
            details.emplace("numeric_address", addr.to_v4().to_uint());

        return {std::move(details), oss.str()};
    }

    static Timeout heartbeatInterval(const TcpHost& settings)
    {
        return settings.heartbeatInterval();
    }

    static Timeout heartbeatInterval(const TcpEndpoint&)
    {
        return unspecifiedTimeout;
    }

    static ErrorOr<SslContextType> makeClientSslContext(const ClientSettings&)
    {
        return {};
    }

    static Socket makeClientSocket(IoStrand i, SslContextType&)
    {
        return Socket{std::move(i)};
    }

    static std::error_code initializeClientSocket(Socket&,
                                                  const ClientSettings&)
    {
        return {};
    }

    static bool isSslTruncationError(boost::system::error_code) {return false;}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TCPTRAITS_HPP
