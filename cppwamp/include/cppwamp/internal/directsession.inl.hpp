/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../directsession.hpp"
#include "../api.hpp"
#include "../router.hpp"
#include "directpeer.hpp"

namespace wamp
{

CPPWAMP_INLINE DirectSession::DirectSession(Executor exec)
    : Base(std::make_shared<internal::DirectPeer>(), std::move(exec))
{}

CPPWAMP_INLINE DirectSession::DirectSession(Executor exec,
                                            FallbackExecutor fallbackExec)
    : Base(std::make_shared<internal::DirectPeer>(), std::move(exec),
           std::move(fallbackExec))
{}

/** @pre `this->state() == SessionState::disconnected`
    @throws error::Logic if the precondition is not met. */
CPPWAMP_INLINE void DirectSession::connect(DirectRouterLink router)
{
    CPPWAMP_LOGIC_CHECK(state() == State::disconnected,
                        "wamp::DirectionSession::connect: Invalid state");
    Base::directConnect(std::move(router));
}

} // namespace wamp
