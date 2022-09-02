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

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

namespace wamp
{

/** Queues and runs I/O completion handlers. */
using IoContext = boost::asio::io_context;

/** Polymorphic executor for I/O objects. */
using AnyIoExecutor = boost::asio::any_io_executor;

/** Serializes I/O operations. */
using IoStrand = boost::asio::strand<AnyIoExecutor>;

/** Metafunction that determines if T meets the requirements of
    Boost.Asio's ExecutionContext. */
template <typename T>
static constexpr bool isExecutionContext()
{
    return std::is_base_of<boost::asio::execution_context, T>::value;
}

/** Completion token used to indicate that there is no completion handler
    waiting for the operation's result. */
using Detached = boost::asio::detached_t;

#if defined(BOOST_ASIO_HAS_CONSTEXPR) || defined(CPPWAMP_FOR_DOXYGEN)
constexpr Detached detached;
#endif

// boost::asio::detached

} // namespace wamp

#endif // CPPWAMP_ASIODEFS_HPP
