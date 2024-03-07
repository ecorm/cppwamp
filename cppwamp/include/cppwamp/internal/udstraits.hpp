/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSTRAITS_HPP
#define CPPWAMP_INTERNAL_UDSTRAITS_HPP

#include <sstream>
#include <tuple>
#include <boost/asio/local/stream_protocol.hpp>
#include "../connectioninfo.hpp"
#include "../traits.hpp"
#include "../transports/udsprotocol.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct UdsTraits
{
    using NetProtocol      = boost::asio::local::stream_protocol;
    using UnderlyingSocket = NetProtocol::socket;
    using Socket           = UnderlyingSocket;
    using ClientSettings   = UdsHost;
    using ServerSettings   = UdsEndpoint;
    using IsTls            = FalseType;
    using SslContextType   = std::tuple<>;

    static ConnectionInfo connectionInfo(const NetProtocol::socket& socket)
    {
        boost::system::error_code ec;
        auto ep = socket.remote_endpoint();
        if (ec)
            ep = {};

        std::ostringstream oss;
        if (ec)
            oss << "Error " << ec;
        else
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

    static SslContextType makeClientSslContext(const ClientSettings&)
    {
        return {};
    }

    static Socket makeClientSocket(IoStrand i, SslContextType&)
    {
        return Socket{std::move(i)};
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSTRAITS_HPP
