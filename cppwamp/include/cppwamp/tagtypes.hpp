/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TAGTYPES_HPP
#define CPPWAMP_TAGTYPES_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contain miscellaneous overload disambiguation tag types. */
//------------------------------------------------------------------------------

#include "api.hpp"
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type used to specify than an operation is to be dispatched via the
    called objects's execution strand.
    Use the wamp::threadSafe constant to conveniently pass this tag
    to functions. */
//------------------------------------------------------------------------------
struct CPPWAMP_API ThreadSafe
{
    constexpr ThreadSafe() = default;
};

//------------------------------------------------------------------------------
/** Constant ThreadSafe object instance that can be passed to functions. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE_VARIABLE constexpr ThreadSafe threadSafe;


} // namespace wamp

#endif // CPPWAMP_TAGTYPES_HPP
