/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASYNCRESULT_HPP
#define CPPWAMP_ASYNCRESULT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Backward compatibility header: use <cppwamp/erroror.hpp> instead. */
//------------------------------------------------------------------------------

#include "config.hpp"
#include "erroror.hpp"

namespace wamp
{

/** @deprecated Backward compatiblity type alias. */
template <typename T>
using AsyncResult CPPWAMP_DEPRECATED = ErrorOr<T>;

} // namespace wamp

#endif // CPPWAMP_ASYNCRESULT_HPP
