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

#include <exception>
#include <boost/asio/spawn.hpp>
#include "asiodefs.hpp"
#include "anyhandler.hpp"
#include "config.hpp"
#include "exceptions.hpp"

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

//------------------------------------------------------------------------------
/** Spawns a coroutine via AnyCompletionExecutor.

    When `CPPWAMP_USE_COMPLETION_YIELD_CONTEXT` is undefined, the
    given executor must have originated from wamp::IoContext or
    wamp::AnyIoExecutor.

    Otherwise, when `CPPWAMP_USE_COMPLETION_YIELD_CONTEXT` is defined, the only
    requirement is that the Boost version be 1.80.0 or above, and that the
    given arguments are supported by boost::asio::spawn.

    wamp::CompletionYieldContext is passed to the function as the yield
    context type. */
//------------------------------------------------------------------------------
template <typename F, typename... Ts>
void spawnCompletionHandler(
    AnyCompletionExecutor& executor, ///< The executor to use.
    F&& function,  ///< Function to run within the coroutine.
    Ts&&... extras ///< Extra arguments passed to boost::asio::spawn
    )
{
#ifdef CPPWAMP_USE_COMPLETION_YIELD_CONTEXT
    spawn(executor, std::forward<F>(function), std::forward<Ts>(extras)...);
#else
    auto* ex1 = executor.template target<typename IoContext::executor_type>();
    if (ex1 != nullptr)
    {
        spawn(*ex1, std::forward<F>(function), std::forward<Ts>(extras)...);
        return;
    }

    auto* ex2 = executor.template target<AnyIoExecutor>();
    if (ex2 != nullptr)
    {
        spawn(*ex2, std::forward<F>(function), std::forward<Ts>(extras)...);
        return;
    }

    CPPWAMP_LOGIC_ERROR("Session::fallbackExecutor() must originate from "
                        "IoContext::executor_type or AnyIoExecutor");
}
#endif

//------------------------------------------------------------------------------
/** Yield context type passed to coroutines launched via
    wamp::spawnCompletionHandler.

    When CPPWAMP_USE_COMPLETION_YIELD_CONTEXT is undefined, this is an alias
    for wamp::YieldContext.

    Otherwise, if CPPWAMP_USE_COMPLETION_YIELD_CONTEXT is defined, this is an
    alias of wamp::BasicYieldContext<wamp::AnyCompletionExecutor>. */
//------------------------------------------------------------------------------
using CompletionYieldContext =
#if CPPWAMP_FOR_DOXYGEN
    BasicYieldContext<UnspecifiedExecutor>;
#elif defined(CPPWAMP_USE_COMPLETION_YIELD_CONTEXT)
    BasicYieldContext<AnyCompletionExecutor>;
#else
    YieldContext;
#endif


//------------------------------------------------------------------------------
/** Completion token type, for Boost.Context-based wamp::spawn, that rethrows
    exceptions thrown by the coroutine. */
//------------------------------------------------------------------------------
struct Propagating
{
    constexpr Propagating() = default;

    void operator()(std::exception_ptr e) const
    {
        if (e) std::rethrow_exception(e);
    }
};

//------------------------------------------------------------------------------
/** Completion token, for Boost.Context-based wamp::spawn, that rethrows
    exceptions thrown by the coroutine. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE_VARIABLE constexpr Propagating propagating;

} // namespace wamp

#endif // CPPWAMP_SPAWN_HPP
