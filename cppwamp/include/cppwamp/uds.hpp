/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UDS_HPP
#define CPPWAMP_UDS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for creating Unix domain socket
           transport connectors. */
//------------------------------------------------------------------------------

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include "api.hpp"
#include "asiodefs.hpp"
#include "connector.hpp"
#include "traits.hpp"
#include "udspath.hpp"

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/asioconnector.hpp"
#include "internal/rawsockconnector.hpp"
#include "internal/udsopener.hpp"
#endif

namespace wamp
{

//------------------------------------------------------------------------------
/** Creates a Connector that can establish a Unix domain socket transport.

    This overload takes an executor that is convertible to
    the boost::asio::any_io_executor polymorphic wrapper.

    @relates UdsPath
    @returns a `std::shared_ptr` to a Connector
    @tparam TCodec The serialization to use over this transport.
    @see Connector, Json, Msgpack */
//------------------------------------------------------------------------------
template <typename TCodec>
CPPWAMP_API Connector::Ptr connector(
    AnyIoExecutor exec, ///< The executor to be used by the transport.
    UdsPath path      ///< Unix domain socket path and other socket options.
);

#ifndef CPPWAMP_COMPILED_LIB
template <typename TCodec>
CPPWAMP_API Connector::Ptr connector(AnyIoExecutor exec, UdsPath path)
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<Endpoint>;
    return ConcreteConnector::create(exec, BufferCodecBuilder{TCodec{}},
                                     std::move(path));
}
#endif


//------------------------------------------------------------------------------
/** Creates a Connector that can establish a TCP raw socket transport.

    Only participates in overload resolution when
    `isExecutionContext<TExecutionContext>() == true`

    @relates TcpHost
    @returns a `std::shared_ptr` to a Connector
    @tparam TCodec The serialization to use over this transport.
    @tparam TExecutionContext The given execution context type (deduced).
    @see Connector, Json, Msgpack */
//------------------------------------------------------------------------------
template <typename TCodec, typename TExecutionContext>
CPPWAMP_ENABLED_TYPE(Connector::Ptr, isExecutionContext<TExecutionContext>())
connector(
    TExecutionContext& context, /**< The I/O context containing the executor
                                     to be used by the transport. */
    UdsPath path ///< Unix domain socket path and other socket options.
)
{
    return connector<TCodec>(context.get_executor(), std::move(path));
}

} // namespace wamp

#endif // CPPWAMP_UDS_HPP
