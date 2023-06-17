/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRAITS_HPP
#define CPPWAMP_TRAITS_HPP

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <vector>

//------------------------------------------------------------------------------
/** @file
    @brief Contains general-purpose type traits. */
//------------------------------------------------------------------------------

/* Hides Needs decorations in order to keep the generated Doxygen
   documentation clean. */
#ifdef CPPWAMP_FOR_DOXYGEN
#define CPPWAMP_NEEDS(cond)
#define CPPWAMP_ENABLED_TYPE(type, cond) type
#else
#define CPPWAMP_NEEDS(cond) Needs<(cond)>
#define CPPWAMP_ENABLED_TYPE(type, cond) Needs<(cond), type>
#endif

namespace wamp
{

//------------------------------------------------------------------------------
/** Metafunction used to enable overloads based on a boolean condition. */
//------------------------------------------------------------------------------
template<bool B, typename T = int>
using Needs = typename std::enable_if<B,T>::type;

//------------------------------------------------------------------------------
/** Metafunction used to obtain the plain value type of a parameter
    passed by universal reference. */
//------------------------------------------------------------------------------
template <typename T>
using ValueTypeOf =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;

//------------------------------------------------------------------------------
/** Pre C++14 substitute for std::decay_t. */
//------------------------------------------------------------------------------
template <typename T>
using Decay = typename std::decay<T>::type;

//------------------------------------------------------------------------------
/** Determines if a type is the same as another. */
//------------------------------------------------------------------------------
template<typename T, typename U>
constexpr bool isSameType() {return std::is_same<T, U>::value;}

//------------------------------------------------------------------------------
/** Determines if the given type is considered a boolean. */
//------------------------------------------------------------------------------
template <typename T>
constexpr bool isBoolLike()
{
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
constexpr bool isNumber()
{
    return std::is_arithmetic<T>::value && !isSameType<T, bool>();
}

//------------------------------------------------------------------------------
/** Determines if the given type is a signed integer. */
//------------------------------------------------------------------------------
template <typename T>
constexpr bool isSignedInteger()
{
    return std::is_integral<T>::value && std::is_signed<T>::value &&
           !isSameType<T, bool>();
}

//------------------------------------------------------------------------------
/** Determines if the given type is an unsigned integer. */
//------------------------------------------------------------------------------
template <typename T>
constexpr bool isUnsignedInteger()
{
    return std::is_integral<T>::value && !std::is_signed<T>::value &&
           !isSameType<T, bool>();
}

//------------------------------------------------------------------------------
/** Equivalent to std::bool_constant provided in C++17. */
//------------------------------------------------------------------------------
template <bool B>
using MetaBool = std::integral_constant<bool, B>;

//------------------------------------------------------------------------------
/** Equivalent to std::true_type. */
//------------------------------------------------------------------------------
using TrueType = std::true_type;

//------------------------------------------------------------------------------
/** Equivalent to std::false_type. */
//------------------------------------------------------------------------------
using FalseType = std::false_type;


//------------------------------------------------------------------------------
/** Pre C++14 substitute for std::conditional_t. */
//------------------------------------------------------------------------------
template <bool B, typename T, typename F>
using Conditional = typename std::conditional<B, T, F>::type;


namespace internal
{
template <typename... Ts>
struct MakeVoid
{
    using type = void;
};
} // namespace internal

//------------------------------------------------------------------------------
/** Pre C++17 substitute for std::void_t. */
//------------------------------------------------------------------------------
template <typename... Ts>
using VoidType = typename internal::MakeVoid<Ts...>::type;


namespace internal
{
template <typename Default, typename, template <typename...> class Operation,
         typename... Args>
struct Detector : FalseType
{};

template <typename Default, template <typename...> class Operation,
         typename... Args>
struct Detector<Default, VoidType<Operation<Args...>>, Operation, Args...>
    : TrueType {};
} // namespace internal

//------------------------------------------------------------------------------
/** Implementation of std::experimental::is_detected. */
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4436.pdf
//------------------------------------------------------------------------------
template <template <typename...> class Operation, typename... Args>
using IsDetected =
    typename internal::Detector<void, void, Operation, Args...>::type;

template <template <typename...> class Operation, typename... Args>
constexpr bool isDetected() noexcept
{
    return internal::Detector<void, void, Operation, Args...>::value;
}


namespace internal
{
// Inspired by
// https://github.com/acmorrow/error_or/blob/master/detail/is_nothrow_swappable.hpp
namespace swap_traits
{
using std::swap;

template <typename T, typename U>
using SwapFn = decltype(swap(std::declval<T&>(), std::declval<U&>()));

template <typename T, typename U>
struct SwapExists : IsDetected<SwapFn, T, U>
{};

template <bool, typename T, typename U>
struct IsNothrow
    : MetaBool<noexcept(swap(std::declval<T&>(), std::declval<U&>()))>
{};

template <typename T, typename U>
struct IsNothrow<false, T, U> : FalseType
{};

} // namespace swap_traits

} // namespace internal

//------------------------------------------------------------------------------
/** Pre-C++17 substitute for std::is_swappable. */
//------------------------------------------------------------------------------
template <typename T, typename U = T>
struct IsSwappable : internal::swap_traits::SwapExists<T, U>
{};

//------------------------------------------------------------------------------
/** Pre-C++17 substitute for std::is_swappable_v. */
//------------------------------------------------------------------------------
template <typename T, typename U = T>
constexpr bool isSwappable() noexcept
{
    return IsSwappable<T, U>::value;
}

//------------------------------------------------------------------------------
/** Pre-C++17 substitute for std::is_nothrow_swappable. */
//------------------------------------------------------------------------------
template <typename T, typename U = T>
struct IsNothrowSwappable
    : internal::swap_traits::IsNothrow<isSwappable<T, U>(), T, U>
{};

//------------------------------------------------------------------------------
/** Pre-C++17 substitute for std::is_nothrow_swappable_v. */
//------------------------------------------------------------------------------
template <typename T, typename U = T>
constexpr bool isNothrowSwappable() noexcept
{
    return IsNothrowSwappable<T, U>::value;
}

//------------------------------------------------------------------------------
/** Pre-C++14 substitute for std::index_sequence. */
//------------------------------------------------------------------------------
template <std::size_t ...> struct IndexSequence { };


#ifndef CPPWAMP_FOR_DOXYGEN
namespace internal
{
// https://stackoverflow.com/a/7858971
template <std::size_t N, std::size_t ...S>
struct GenIndexSequence : GenIndexSequence<N-1, N-1, S...> { };

template <std::size_t ...S>
struct GenIndexSequence<0, S...>
{
    using type = IndexSequence<S...>;
};
} // namespace internal
#endif // CPPWAMP_FOR_DOXYGEN


//------------------------------------------------------------------------------
/** Pre-C++14 substitute for std::make_index_sequence. */
//------------------------------------------------------------------------------
template <std::size_t N>
using MakeIndexSequence = typename internal::GenIndexSequence<N>::type;

//------------------------------------------------------------------------------
/** Pre-C++14 substitute for std::index_sequence_for. */
//------------------------------------------------------------------------------
template <typename... Ts>
using IndexSequenceFor = MakeIndexSequence<sizeof...(Ts)>;

} // namespace wamp

#endif // CPPWAMP_TRAITS_HPP
