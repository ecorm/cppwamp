/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "callee.hpp"
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Registration::~Registration() {unregister();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const Procedure& Registration::procedure() const
{
    return procedure_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE RegistrationId Registration::id() const {return id_;}

//------------------------------------------------------------------------------
/** @note Duplicate unregistrations are safely ignored. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Registration::unregister()
{
    auto callee = callee_.lock();
    if (callee)
        callee->unregister(id_);
}

//------------------------------------------------------------------------------
/** @details
    Equivalent to Session::unregister(Registration::Ptr, AsyncHandler<bool>).
    @note Duplicate unregistrations are safely ignored. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Registration::unregister(AsyncHandler<bool> handler)
{
    auto callee = callee_.lock();
    if (callee)
        callee->unregister(id_, std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Registration::Registration(CalleePtr callee,
                                          Procedure&& procedure)
    : callee_(callee),
      procedure_(std::move(procedure))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Registration::setId(RegistrationId id, internal::PassKey)
{
    id_ = id;
}

} // namespace wamp
