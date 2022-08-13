/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRAITS_HPP
#define CPPWAMP_TRAITS_HPP

#include <tuple>
#include <type_traits>
#include <vector>
#include "api.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains general-purpose type traits. */
//------------------------------------------------------------------------------

/* Hides EnableIf decorations in order to keep the generated Doxygen
   documentation clean. */
#ifdef CPPWAMP_FOR_DOXYGEN
#define CPPWAMP_ENABLE_IF(cond)
#define CPPWAMP_ENABLED_TYPE(type, cond) type
#else
#define CPPWAMP_ENABLE_IF(cond) EnableIf<(cond)>
#define CPPWAMP_ENABLED_TYPE(type, cond) EnableIf<(cond), type>
#endif

namespace wamp
{

//------------------------------------------------------------------------------
/** Metafunction used to enable overloads based on a boolean condition. */
//------------------------------------------------------------------------------
template<bool B, typename T = int>
using EnableIf = typename std::enable_if<B,T>::type;

//------------------------------------------------------------------------------
/** Metafunction used to disable overloads based on a boolean condition. */
//------------------------------------------------------------------------------
template<bool B, typename T = int>
using DisableIf = typename std::enable_if<!B,T>::type;

//------------------------------------------------------------------------------
/** Metafunction used to obtain the plain value type of a parameter
    passed by universal reference. */
//------------------------------------------------------------------------------
template <typename T>
using ValueTypeOf =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;

//------------------------------------------------------------------------------
/** Determines if a type is the same as another. */
//------------------------------------------------------------------------------
template<typename T, typename U>
CPPWAMP_API constexpr bool isSameType() {return std::is_same<T, U>::value;}

//------------------------------------------------------------------------------
/** Determines if the given type is considered a boolean. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API constexpr bool isBool()
{
    // std::vector<bool>::const_reference is not just bool in clang/libc++.
    return isSameType<T, bool>() ||
           isSameType<T, std::vector<bool>::reference>() ||
           isSameType<T, std::vector<bool>::const_reference>();
}

//------------------------------------------------------------------------------
/** Determines if the given type is considered a number.
    @note To be consitent with Javascript's strict equality, a boolean is not
    considered a number, */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API constexpr bool isNumber()
{
    return std::is_arithmetic<T>::value && !isSameType<T, bool>();
}

//------------------------------------------------------------------------------
/** Determines if the given type is a signed integer. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API constexpr bool isSignedInteger()
{
    return std::is_integral<T>::value && std::is_signed<T>::value &&
           !isSameType<T, bool>();
}

//------------------------------------------------------------------------------
/** Determines if the given type is an unsigned integer. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API constexpr bool isUnsignedInteger()
{
    return std::is_integral<T>::value && !std::is_signed<T>::value &&
           !isSameType<T, bool>();
}

//------------------------------------------------------------------------------
/** Metafunction that obtains the Nth type of a parameter pack. */
//------------------------------------------------------------------------------
template<int N, typename... Ts> using NthTypeOf =
    typename std::tuple_element<N, std::tuple<Ts...>>::type;

//------------------------------------------------------------------------------
/** Equivalent to std::bool_constant provided in C++17. */
//------------------------------------------------------------------------------
template <bool B>
using BoolConstant = std::integral_constant<bool, B>;

//------------------------------------------------------------------------------
/** Equivalent to std::true_type provided in C++17. */
//------------------------------------------------------------------------------
using TrueType = BoolConstant<true>;

//------------------------------------------------------------------------------
/** Equivalent to std::false_type provided in C++17. */
//------------------------------------------------------------------------------
using FalseType = BoolConstant<false>;

} // namespace wamp

#endif // CPPWAMP_TRAITS_HPP
