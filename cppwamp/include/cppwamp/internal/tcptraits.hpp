/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPTRAITS_HPP
#define CPPWAMP_INTERNAL_TCPTRAITS_HPP

#include <sstream>
#include <boost/asio/ip/tcp.hpp>
#include "../connectioninfo.hpp"
#include "../timeout.hpp"
#include "../transports/tcpprotocol.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct TcpTraits
{
    using NetProtocol = boost::asio::ip::tcp;
    using ClientSettings = TcpHost;
    using ServerSettings = TcpEndpoint;

    template <typename TEndpoint>
    static ConnectionInfo connectionInfo(const TEndpoint& ep,
                                         const std::string& server)
    {
        static constexpr unsigned ipv4VersionNo = 4;
        static constexpr unsigned ipv6VersionNo = 6;

        std::ostringstream oss;
        oss << ep;
        const auto addr = ep.address();
        const bool isIpv6 = addr.is_v6();

        Object details
        {
            {"address", addr.to_string()},
            {"ip_version", isIpv6 ? ipv6VersionNo : ipv4VersionNo},
            {"endpoint", oss.str()},
            {"port", ep.port()},
            {"protocol", "TCP"},
        };

        if (!isIpv6)
            details.emplace("numeric_address", addr.to_v4().to_uint());

        return {std::move(details), oss.str(), server};
    }

    static Timeout heartbeatInterval(const TcpHost& settings)
    {
        return settings.heartbeatInterval();
    }

    static Timeout heartbeatInterval(const TcpEndpoint&)
    {
        return unspecifiedTimeout;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TCPTRAITS_HPP
