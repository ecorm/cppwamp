/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <cppwamp/internal/config.hpp>

#ifdef CPPWAMP_COMPILED_LIB

#include <cppwamp/dialoguedata.hpp>
#include <cppwamp/error.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/rawsockoptions.hpp>
#include <cppwamp/registration.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/sessiondata.hpp>
#include <cppwamp/subscription.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/tcphost.hpp>
#include <cppwamp/udspath.hpp>
#include <cppwamp/version.hpp>
#include <cppwamp/internal/messagetraits.hpp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
#include <cppwamp/uds.hpp>
#endif

#include <cppwamp/internal/dialoguedata.ipp>
#include <cppwamp/internal/error.ipp>
#include <cppwamp/internal/messagetraits.ipp>
#include <cppwamp/internal/rawsockoptions.ipp>
#include <cppwamp/internal/registration.ipp>
#include <cppwamp/internal/session.ipp>
#include <cppwamp/internal/sessiondata.ipp>
#include <cppwamp/internal/subscription.ipp>
#include <cppwamp/internal/tcp.ipp>
#include <cppwamp/internal/tcphost.ipp>
#include <cppwamp/internal/udspath.ipp>
#include <cppwamp/internal/version.ipp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
#include <cppwamp/internal/uds.ipp>
#endif

// Explicit template instantiations
namespace wamp
{

template Connector::Ptr connector<Json>(AsioService& iosvc, TcpHost host);
template Connector::Ptr connector<Msgpack>(AsioService& iosvc, TcpHost host);

namespace legacy
{
    template Connector::Ptr connector<Json>(AsioService& iosvc, TcpHost host);
    template Connector::Ptr connector<Msgpack>(AsioService& iosvc, TcpHost host);
}

namespace internal
{
    template void applyRawsockOptions(
        const TcpHost& options,
        boost::asio::ip::tcp::socket& socket);
}

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
template Connector::Ptr connector<Json>(AsioService& iosvc, UdsPath path);
template Connector::Ptr connector<Msgpack>(AsioService& iosvc, UdsPath path);

namespace legacy
{
    template Connector::Ptr connector<Json>(AsioService& iosvc, UdsPath path);
    template Connector::Ptr connector<Msgpack>(AsioService& iosvc, UdsPath path);
}

namespace internal
{
    template void applyRawsockOptions(
        const UdsPath& options,
        boost::asio::local::stream_protocol::socket& socket);
}
#endif

} // namespace wamp


#endif
