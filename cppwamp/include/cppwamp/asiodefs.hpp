/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASIODEFS_HPP
#define CPPWAMP_ASIODEFS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Commonly used Boost.Asio type aliases.
    @see <cppwamp/spawn.hpp> */
//------------------------------------------------------------------------------

#include <type_traits>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include "config.hpp"

namespace wamp
{

/** Polymorphic executor for I/O objects. */
using AnyIoExecutor = boost::asio::any_io_executor;

/** Alias of AnyIoExecutor kept for backward compatibility.
    @deprecated Use wamp::AnyIoExecutor instead. */
using AnyExecutor CPPWAMP_DEPRECATED = AnyIoExecutor;

/** Queues and runs I/O completion handlers. */
using IoContext = boost::asio::io_context;

/** Alias of IoContext kept for backward compatibility. */
using AsioContext CPPWAMP_DEPRECATED = boost::asio::io_context;

/** Alias of AsioContext kept for backward compatibility.
    @deprecated Use wamp::AsioContext instead. */
using AsioService CPPWAMP_DEPRECATED = IoContext;

/** Serializes I/O operations. */
using IoStrand = boost::asio::strand<AnyIoExecutor>;

/** Type used by Boost.Asio for reporting errors.
    @deprecated Will be removed */
using AsioErrorCode = boost::system::error_code;

/** Metafunction that determines if T meets the requirements of
    Boost.Asio's ExecutionContext. */
template <typename T>
static constexpr bool isExecutionContext()
{
    return std::is_base_of<boost::asio::execution_context, T>::value;
}

} // namespace wamp

#endif // CPPWAMP_ASIODEFS_HPP
