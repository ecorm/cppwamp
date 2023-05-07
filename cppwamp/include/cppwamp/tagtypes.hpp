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

#ifdef __has_include
#   if(__has_include(<version>))
#       include <version>
#   elif(__has_include(<any>))
#       include <any>
#   elif(__has_include(<optional>))
#       include <optional>
#   elif(__has_include(<variant>))
#       include <variant>
#   endif
#endif

namespace wamp
{

#if defined(__cpp_lib_any) || defined(__cpp_lib_optional) || \
    defined(__cpp_lib_variant) || defined(CPPWAMP_FOR_DOXYGEN)

//------------------------------------------------------------------------------
/** Alias to std::in_place_t if available, otherwise emulates it. */
//------------------------------------------------------------------------------
using in_place_t = std::in_place_t;

//------------------------------------------------------------------------------
/** Alias to std::in_place_type_t if available, otherwise emulates it. */
//------------------------------------------------------------------------------
template <typename T>
using in_place_type_t = std::in_place_type_t<T>;

#else

struct CPPWAMP_API in_place_t { constexpr explicit in_place_t() = default; };

template <typename T>
struct in_place_type_t {constexpr explicit in_place_type_t() = default;};

#endif

//------------------------------------------------------------------------------
/** Alias to std::in_place if available, otherwise emulates it. */
//------------------------------------------------------------------------------
CPPWAMP_API CPPWAMP_INLINE_VARIABLE constexpr in_place_t in_place{};


#if defined(__cpp_variable_templates) || defined(CPPWAMP_FOR_DOXYGEN)

//------------------------------------------------------------------------------
/** Alias to std::in_place_type if available, otherwise emulates it. */
//------------------------------------------------------------------------------
template <typename T>
constexpr in_place_type_t<T> in_place_type{};

#endif


} // namespace wamp

#endif // CPPWAMP_TAGTYPES_HPP
