/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_DIRECTSESSION_HPP
#define CPPWAMP_DIRECTSESSION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the DirectSession class. */
//------------------------------------------------------------------------------

#include "router.hpp"
#include "session.hpp"

namespace wamp
{

class Router;

//------------------------------------------------------------------------------
class CPPWAMP_API DirectSession : public Session
{
public:
    /** Constructor taking an executor. */
    explicit DirectSession(Executor exec);

    /** Constructor taking an executor for I/O operations
        and another for user-provided handlers. */
    DirectSession(const Executor& exec, FallbackExecutor fallbackExec);

    /** Constructor taking an execution context. */
    template <typename E, CPPWAMP_NEEDS(isExecutionContext<E>()) = 0>
    explicit DirectSession(E& context)
        : DirectSession(context.get_executor())
    {}

    /** Constructor taking an I/O execution context and another as fallback
        for user-provided handlers. */
    template <typename E1, typename E2,
             CPPWAMP_NEEDS(isExecutionContext<E1>() &&
                           isExecutionContext<E2>()) = 0>
    explicit DirectSession(E1& executionContext, E2& fallbackExecutionContext)
        : DirectSession(executionContext.get_executor(),
                        fallbackExecutionContext.get_executor())
    {}

    /** Connects directly to a router. */
    void connect(DirectRouterLink router);

private:
    using Base = Session;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/directsession.inl.hpp"
#endif

#endif // CPPWAMP_DIRECTSESSION_HPP
