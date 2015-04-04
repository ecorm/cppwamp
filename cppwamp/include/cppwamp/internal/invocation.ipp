/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "../error.hpp"
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE RequestId Invocation::requestId() const {return id_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Invocation::calleeHasExpired() const
{
    return callee_.expired();
}

//------------------------------------------------------------------------------
/** @pre `this->calleeHasExpired == false`
    @pre A result was not already sent back to the callee for this invocation */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Invocation::yield()
{
    CPPWAMP_LOGIC_CHECK(!calleeHasExpired(), "Client no longer exists");
    CPPWAMP_LOGIC_CHECK(!hasReturned_, "Invocation has already returned");
    callee_.lock()->yield(id_);
    hasReturned_ = true;
}

//------------------------------------------------------------------------------
/** @pre `this->calleeHasExpired == false`
    @pre A result was not already sent back to the callee for this invocation */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Invocation::yield(Args result)
{
    CPPWAMP_LOGIC_CHECK(!calleeHasExpired(), "Client no longer exists");
    CPPWAMP_LOGIC_CHECK(!hasReturned_, "Invocation has already returned");
    callee_.lock()->yield(id_, std::move(result));
    hasReturned_ = true;
}

//------------------------------------------------------------------------------
/** @pre `this->calleeHasExpired == false`
    @pre A result was not already sent back to the callee for this invocation */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Invocation::fail(std::string reason, Object details,
                                     Args args)
{
    CPPWAMP_LOGIC_CHECK(!calleeHasExpired(), "Client no longer exists");
    CPPWAMP_LOGIC_CHECK(!hasReturned_, "Invocation has already returned");
    using std::move;
    callee_.lock()->fail(id_, move(reason), move(details), move(args));
    hasReturned_ = true;
}

//------------------------------------------------------------------------------
/** @pre `this->calleeHasExpired == false`
    @pre A result was not already sent back to the callee for this invocation */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Invocation::fail(std::string reason, Args args)
{
    fail(std::move(reason), Object(), std::move(args));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Invocation::Invocation(CalleePtr callee, RequestId id)
    : callee_(callee), id_(id), hasReturned_(false)
{}

} // namespace wamp
