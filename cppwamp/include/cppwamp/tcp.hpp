/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TCP_HPP
#define CPPWAMP_TCP_HPP

//------------------------------------------------------------------------------
/** @file
    Contains facilities for creating TCP transport connectors. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "asiodefs.hpp"
#include "connector.hpp"
#include "tcphost.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Creates a Connector that can establish a TCP raw socket transport.
    @relates TcpHost
    @returns a `std::shared_ptr` to a Connector
    @tparam TCodec The serialization to use over this transport.
    @see Connector, Json, Msgpack */
//------------------------------------------------------------------------------
template <typename TCodec>
Connector::Ptr connector(
    AsioService& iosvc, ///< The I/O service to be used by the transport.
    TcpHost host        ///< TCP host address and other socket options.
);


//------------------------------------------------------------------------------
/** Creates a Connector that can establish a TCP raw socket transport on
    non-conformant routers.
    This is an interim Connector for connecting to routers that do not yet
    support handshaking on their raw socket transports. Handshaking was
    introduced in [version e2c4e57][e2c4e57] of the advanced WAMP specification.
    [e2c4e57]: https://github.com/tavendo/WAMP/commit/e2c4e5775d89fa6d991eb2e138e2f42ca2469fa8
    @relates TcpHost
    @returns a `std::shared_ptr` to a Connector
    @tparam TCodec The serialization to use over this transport.
    @see Connector, Json, Msgpack, makeUds, makeLegacyTcp */
//------------------------------------------------------------------------------
template <typename TCodec>
Connector::Ptr legacyConnector(
    AsioService& iosvc, ///< The I/O service to be used by the transport.
    TcpHost host        ///< TCP host address and other socket options.
);

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/tcp.ipp"
#endif

#endif // CPPWAMP_TCP_HPP
