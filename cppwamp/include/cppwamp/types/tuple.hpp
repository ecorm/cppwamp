/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONVERTERS_TUPLE_HPP
#define CPPWAMP_CONVERTERS_TUPLE_HPP

//------------------------------------------------------------------------------
/** @file
    Provides facilities allowing Variant to interoperate with std::tuple. */
//------------------------------------------------------------------------------

#include <sstream>
#include <type_traits>
#include "../conversion.hpp"
#include "../internal/integersequence.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Converts a Variant::Array to a `std::tuple`.
    @pre The Array's element types must be convertible to the
         tuple's element types.
    @throws error::Conversion if one of the Array element types is not
            convertible to the target type. */
//------------------------------------------------------------------------------
template <typename... Ts>
void toTuple(const wamp::Variant::Array& array, std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Converts a `std::tuple` to a Variant::Array.
    @pre The tuple values must be convertible to Variant's bound types
         (statically checked). */
//------------------------------------------------------------------------------
template <typename... Ts>
wamp::Variant::Array toArray(const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Performs the conversion from an array variant to a `std::tuple`.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename... Ts>
void convert(FromVariantConverter& conv, std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Performs the conversion from a `std::tuple` to an array variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename... Ts>
void convert(ToVariantConverter& conv, std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a Variant::Array and a `std::tuple` for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const Array& array, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant::Array for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const std::tuple<Ts...>& tuple, const Array& array);

//------------------------------------------------------------------------------
/** Compares a Variant::Array and a `std::tuple` for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const Array& array, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant::Array for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const std::tuple<Ts...>& tuple, const Array& array);

//------------------------------------------------------------------------------
/** Compares a Variant and a `std::tuple` for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const Variant& variant, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const std::tuple<Ts...>& tuple, const Variant& variant);

//------------------------------------------------------------------------------
/** Compares a Variant and a `std::tuple` for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const Variant& variant, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const std::tuple<Ts...>& tuple, const Variant& variant);

} // namespace wamp

#include "./internal/tuple.ipp"

#endif // CPPWAMP_CONVERTERS_TUPLE_HPP
