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

#include <memory>
#include <utility>
#include "api.hpp"
#include "asiodefs.hpp"
#include "codec.hpp"
#include "connector.hpp"
#include "udspath.hpp"

namespace wamp
{

// TODO: Doxygen
//------------------------------------------------------------------------------
class CPPWAMP_API UdsConnector : public Connecting
{
public:
    using Ptr = std::shared_ptr<UdsConnector>;

    static Ptr create(const AnyIoExecutor& e, UdsPath p, BufferCodecBuilder b);

    IoStrand strand() const override;

protected:
    Connecting::Ptr clone() const override;

    void establish(Handler&& handler) override;

    void cancel()  override;

private:
    CPPWAMP_HIDDEN UdsConnector(IoStrand s, UdsPath p, BufferCodecBuilder b);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** Creates a Connector that can establish a Unix domain socket transport.

    This overload takes an executor that is convertible to
    the boost::asio::any_io_executor polymorphic wrapper.

    @relates UdsPath
    @returns a `std::shared_ptr` to a Connecting
    @tparam TFormat The serialization format to use over this transport.
    @see Connecting, Json, Msgpack */
//------------------------------------------------------------------------------
template <typename TFormat>
CPPWAMP_API UdsConnector::Ptr connector(
    const AnyIoExecutor& e, ///< The executor to be used by the transport.
    UdsPath p ///< Unix domain socket path and other socket options.
)
{
    return UdsConnector::create(e, std::move(p), BufferCodecBuilder{TFormat{}});
}


//------------------------------------------------------------------------------
/** Creates a Connector that can establish a TCP raw socket transport.

    Only participates in overload resolution when
    `isExecutionContext<TExecutionContext>() == true`

    @relates TcpHost
    @returns a `std::shared_ptr` to a Connecting
    @tparam TFormat The serialization formatto use over this transport.
    @tparam TExecutionContext The given execution context type (deduced).
    @see Connecting, Json, Msgpack */
//------------------------------------------------------------------------------
template <typename TFormat, typename TExecutionContext>
CPPWAMP_ENABLED_TYPE(UdsConnector::Ptr, isExecutionContext<TExecutionContext>())
connector(
    TExecutionContext& context, /**< The I/O context containing the executor
                                     to be used by the transport. */
    UdsPath path ///< Unix domain socket path and other socket options.
)
{
    return connector<TFormat>(context.get_executor(), std::move(path));
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/uds.ipp"
#endif

#endif // CPPWAMP_UDS_HPP
