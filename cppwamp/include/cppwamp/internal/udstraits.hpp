/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSTRAITS_HPP
#define CPPWAMP_INTERNAL_UDSTRAITS_HPP

#include <sstream>
#include <boost/asio/local/stream_protocol.hpp>
#include "../connectioninfo.hpp"
#include "../transports/udsprotocol.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct UdsTraits
{
    using NetProtocol = boost::asio::local::stream_protocol;
    using ClientSettings = UdsHost;
    using ServerSettings = UdsEndpoint;

    static ConnectionInfo connectionInfo(const NetProtocol::socket& socket)
    {
        auto ep = socket.remote_endpoint();
        std::ostringstream oss;
        oss << ep;

        Object details
        {
            {"endpoint", oss.str()},
            {"path", ep.path()},
            {"protocol", "UDS"},
        };

        return {std::move(details), oss.str()};
    }

    static Timeout heartbeatInterval(const UdsHost&)
    {
        return unspecifiedTimeout;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSTRAITS_HPP
