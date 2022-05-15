/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASIODEFS_HPP
#define CPPWAMP_ASIODEFS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Common type definitions used by transports
           that rely on Boost.Asio. */
//------------------------------------------------------------------------------

#include <type_traits>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

namespace wamp
{

/** Polymorphic executor for all I/O objects. */
using AnyExecutor = boost::asio::any_io_executor;

/** Queues and runs I/O completion handlers. */
using AsioContext = boost::asio::io_context;

/** Alias of AsioContext kept for backward compatibility.
    @deprecated Use wamp::AsioContext instead. */
using AsioService = AsioContext;

/** Type used by Boost.Asio for reporting errors. */
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
