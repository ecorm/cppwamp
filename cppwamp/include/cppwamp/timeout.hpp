/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TIMEOUT_HPP
#define CPPWAMP_TIMEOUT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains definitions and constants related to timeouts. */
//------------------------------------------------------------------------------

#include <chrono>
#include "exceptions.hpp"

namespace wamp
{

/** Duration type used for general timeouts. */
using Timeout = std::chrono::steady_clock::duration;

/** Special value indicating the timeout duration is not specified. */
static constexpr Timeout unspecifiedTimeout{0};

/** Special value indicating the operation is to wait indefinitely
    for completion. */
static constexpr Timeout neverTimeout{Timeout::max()};


namespace internal
{

inline Timeout checkTimeout(Timeout t)
{
    CPPWAMP_LOGIC_CHECK(t.count() >= 0, "Timeout cannot be negative");
    return t;
}

constexpr bool timeoutIsDefinite(Timeout t)
{
    return t != unspecifiedTimeout && t != neverTimeout;
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_TIMEOUT_HPP
