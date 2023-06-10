/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPTRAITS_HPP
#define CPPWAMP_INTERNAL_TCPTRAITS_HPP

#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct TcpTraits
{
    template <typename TEndpoint>
    static Object remoteEndpointDetails(const TEndpoint& ep)
    {
        auto addr = ep.address();
        bool isIpv6 = addr.is_v6();

        Object details
        {
            {"address", addr.to_string()},
            {"ip_version", isIpv6 ? 6 : 4},
            {"port", ep.port()},
            {"protocol", "TCP"},
        };

        if (!isIpv6)
        {
            details.emplace("numeric_address", addr.to_v4().to_uint());
        }

        return details;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TCPTRAITS_HPP
