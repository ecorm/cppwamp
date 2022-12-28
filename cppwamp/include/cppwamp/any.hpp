/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ANY_HPP
#define CPPWAMP_ANY_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the Any class. */
//------------------------------------------------------------------------------

#if defined(__has_include) && __has_include(<any>)
#   include <any>
#   if defined(__cpp_lib_any)
#       define CPPWAMP_HAS_ANY 1
#   endif
#endif

#ifdef CPPWAMP_HAS_ANY

namespace wamp
{

template <typename T>
using in_place_type_t = std::in_place_type_t<T>;

template <typename T>
constexpr in_place_type_t<T> in_place_type{};

using bad_any_cast = std::bad_any_cast;

using any = std::any;

} // namespace wamp

#else

#include "internal/surrogateany.hpp"

namespace wamp
{

template <typename T>
using in_place_type_t = internal::InPlaceType<T>;

#ifdef __cpp_variable_templates
template <typename T>
constexpr in_place_type_t<T> in_place_type{};
#endif

using bad_any_cast = internal::BadAnyCast;

using any = internal::SurrogateAny;

inline void swap(any& lhs, any& rhs) noexcept {lhs.swap(rhs);}

template <typename T>
const T* any_cast(const any* a) noexcept {return internal::anyCast<T>(a);}

template<typename T>
T* any_cast(any* a) noexcept {return internal::anyCast<T>(a);}

template <typename T>
T any_cast(const any& a) {return internal::anyCast<T>(a);}

template <typename T>
T any_cast(any& a) {return internal::anyCast<T>(a);}

template <typename T>
T any_cast(any&& a) {return internal::anyCast<T>(std::move(a));}

template <typename T, typename... As >
any make_any(As&&... args)
{
    return internal::SurrogateAny(internal::inPlaceType<T>,
                                  std::forward<As>(args)...);
}

template <typename T, typename U, typename... As>
any make_any(std::initializer_list<U> il, As&&... args)
{
    return internal::SurrogateAny(internal::inPlaceType<T>, il,
                                  std::forward<As>(args)...);
}

} // namespace wamp

#endif // CPPWAMP_HAS_ANY

#endif // CPPWAMP_ANY_HPP
