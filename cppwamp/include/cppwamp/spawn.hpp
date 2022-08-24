/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SPAWN_HPP
#define CPPWAMP_SPAWN_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for spawning stackful coroutines. */
//------------------------------------------------------------------------------

#include <boost/asio/spawn.hpp>

namespace wamp
{

//------------------------------------------------------------------------------
/** Alias for boost::asio::basic_yield_context imported into the
    `wamp` namespace. */
//------------------------------------------------------------------------------
template <typename TExecutor>
using BasicYieldContext = boost::asio::basic_yield_context<TExecutor>;


//------------------------------------------------------------------------------
/** Alias for boost::asio::yield_context imported into the `wamp` namespace. */
//------------------------------------------------------------------------------
using YieldContext = boost::asio::yield_context;


//------------------------------------------------------------------------------
/** Alias for boost::asio::spawn imported into the `wamp` namespace. */
//------------------------------------------------------------------------------
template <typename... TArgs>
auto spawn(TArgs&&... args)
    -> decltype(boost::asio::spawn(std::forward<TArgs>(args)...))
{
    return boost::asio::spawn(std::forward<TArgs>(args)...);
}

} // namespace wamp

#endif // CPPWAMP_SPAWN_HPP
