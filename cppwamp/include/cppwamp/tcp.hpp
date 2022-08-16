/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TCP_HPP
#define CPPWAMP_TCP_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for creating TCP transport connectors. */
//------------------------------------------------------------------------------

#include <memory>
#include <utility>
#include "api.hpp"
#include "asiodefs.hpp"
#include "codec.hpp"
#include "config.hpp"
#include "connector.hpp"
#include "tcphost.hpp"

namespace wamp
{

// TODO: Doxygen
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Connector<Tcp> : public Connecting
{
public:
    using Ptr = std::shared_ptr<Connector>;

    static Ptr create(IoStrand s, TcpHost h, int codecId);

    void establish(Handler&& handler) override;

    void cancel() override;

private:
    CPPWAMP_HIDDEN Connector(IoStrand s, TcpHost h, int codecId);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

using TcpConnector = Connector<Tcp>;

//------------------------------------------------------------------------------
/** Creates a LegacyConnector that can establish a TCP raw socket transport.

    This overload takes an executor that is convertible to
    the boost::asio::any_io_executor polymorphic wrapper.

    @deprecated Use wamp::TransportWishes instead
    @relates TcpHost
    @returns a `std::shared_ptr` to a Connecting
    @tparam TFormat The serialization format to use over this transport.
    @see Connecting, Json, Msgpack */
//------------------------------------------------------------------------------
template <typename TFormat>
CPPWAMP_API LegacyConnector connector(
    AnyIoExecutor e, ///< The executor to be used by the transport.
    TcpHost h        ///< TCP host address and other socket options.
)
{
    return LegacyConnector{std::move(e), std::move(h), TFormat{}};
}

//------------------------------------------------------------------------------
/** Creates a LegacyConnector that can establish a TCP raw socket transport.

    Only participates in overload resolution when
    `isExecutionContext<TExecutionContext>() == true`

    @deprecated Use wamp::TransportWishes instead
    @relates TcpHost
    @returns a `std::shared_ptr` to a Connecting
    @tparam TFormat The serialization format to use over this transport.
    @tparam TExecutionContext The given execution context type (deduced).
    @see Connecting, Json, Msgpack */
//------------------------------------------------------------------------------
template <typename TFormat, typename TExecutionContext>
CPPWAMP_ENABLED_TYPE(LegacyConnector, isExecutionContext<TExecutionContext>())
connector(
    TExecutionContext& context, /**< The I/O context containing the executor
                                     to be used by the transport. */
    TcpHost host                ///< TCP host address and other socket options.
)
{
    return connector<TFormat>(context.get_executor(), std::move(host));
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/tcp.ipp"
#endif

#endif // CPPWAMP_TCP_HPP
