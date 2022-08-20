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
#include "connector.hpp"
#include "tcphost.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Connector specialization that establishes a TCP transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Connector<Tcp> : public Connecting
{
public:
    /** Type containing the transport settings. */
    using Settings = TcpHost;

    /** Constructor. */
    Connector(IoStrand i, Settings s, int codecId);

    /** Destructor. */
    ~Connector();

    /** Starts establishing the transport connection, emitting a
        Transportable::Ptr via the given handler if successful. */
    void establish(Handler&& handler) override;

    /** Cancels transport connection in progress, emitting an error code
        via the handler passed to the establish method. */
    void cancel() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** Creates a LegacyConnector that can establish a TCP raw socket transport.

    This overload takes an executor that is convertible to
    the boost::asio::any_io_executor polymorphic wrapper.

    @deprecated Use wamp::ConnectionWish instead
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

    @deprecated Use wamp::ConnectionWish instead
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
