/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COMPILED_LIB
#error CPPWAMP_COMPILED_LIB must be defined to use this source file
#endif

#include <cppwamp/internal/cbor.ipp>
#include <cppwamp/internal/json.ipp>
#include <cppwamp/internal/msgpack.ipp>

#include <cppwamp/config.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/internal/asioconnector.hpp>
#include <cppwamp/internal/rawsockconnector.hpp>
#include <cppwamp/internal/tcpopener.hpp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/uds.hpp>
    #include <cppwamp/internal/udsopener.hpp>
#endif

#include <cppwamp/internal/blob.ipp>
#include <cppwamp/internal/chits.ipp>
#include <cppwamp/internal/error.ipp>
#include <cppwamp/internal/messagetraits.ipp>
#include <cppwamp/internal/peerdata.ipp>
#include <cppwamp/internal/registration.ipp>
#include <cppwamp/internal/session.ipp>
#include <cppwamp/internal/subscription.ipp>
#include <cppwamp/internal/tcphost.ipp>
#include <cppwamp/internal/variant.ipp>
#include <cppwamp/internal/version.ipp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/internal/udspath.ipp>
#endif

// Explicit template instantiations
// These are here to avoid leaking jsoncons details to the application,
// which would lengthen its compile time.
namespace wamp
{

template <> CPPWAMP_API Connector::Ptr
connector<Json>(AnyIoExecutor exec, TcpHost host)
{
    using Endpoint = internal::AsioConnector<internal::TcpOpener>;
    using ConcreteConnector = internal::RawsockConnector<Json, Endpoint>;
    return ConcreteConnector::create(exec, std::move(host));
}

template <> CPPWAMP_API Connector::Ptr
connector<Msgpack>(AnyIoExecutor exec, TcpHost host)
{
    using Endpoint = internal::AsioConnector<internal::TcpOpener>;
    using ConcreteConnector = internal::RawsockConnector<Msgpack, Endpoint>;
    return ConcreteConnector::create(exec, std::move(host));
}

template <> CPPWAMP_API Connector::Ptr
connector<Cbor>(AnyIoExecutor exec, TcpHost host)
{
    using Endpoint = internal::AsioConnector<internal::TcpOpener>;
    using ConcreteConnector = internal::RawsockConnector<Cbor, Endpoint>;
    return ConcreteConnector::create(exec, std::move(host));
}

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
template <> CPPWAMP_API Connector::Ptr
connector<Json>(AnyIoExecutor exec, UdsPath path)
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<Json, Endpoint>;
    return ConcreteConnector::create(exec, std::move(path));
}

template <> CPPWAMP_API Connector::Ptr
connector<Msgpack>(AnyIoExecutor exec, UdsPath path)
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<Msgpack, Endpoint>;
    return ConcreteConnector::create(exec, std::move(path));
}

template <> CPPWAMP_API Connector::Ptr
connector<Cbor>(AnyIoExecutor exec, UdsPath path)
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<Cbor, Endpoint>;
    return ConcreteConnector::create(exec, std::move(path));
}
#endif

} // namespace wamp
