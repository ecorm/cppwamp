/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COMPILED_LIB
#error CPPWAMP_COMPILED_LIB must be defined to use this source file
#endif

#include <cppwamp/tcp.hpp>
#include <cppwamp/msgpack/api.hpp>
#include <cppwamp/internal/asioconnector.hpp>
#include <cppwamp/internal/config.hpp>
#include <cppwamp/internal/rawsockconnector.hpp>
#include <cppwamp/internal/tcpopener.hpp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/uds.hpp>
    #include <cppwamp/internal/udsopener.hpp>
#endif

#include <cppwamp/msgpack/internal/msgpack.ipp>

// Explicit template instantiations
// These are here to avoid leaking msgpack-c details to the application,
// which would lengthen its compile time.
namespace wamp
{

template <> CPPWAMP_MSGPACK_API Connector::Ptr
connector<Msgpack>(AnyExecutor exec, TcpHost host)
{
    using Endpoint = internal::AsioConnector<internal::TcpOpener>;
    using ConcreteConnector = internal::RawsockConnector<Msgpack, Endpoint>;
    return ConcreteConnector::create(exec, std::move(host));
}

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
template <> CPPWAMP_MSGPACK_API Connector::Ptr
connector<Msgpack>(AnyExecutor exec, UdsPath path)
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<Msgpack, Endpoint>;
    return ConcreteConnector::create(exec, std::move(path));
}
#endif

} // namespace wamp
