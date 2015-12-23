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

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/tcp.ipp"
#endif

#endif // CPPWAMP_TCP_HPP
