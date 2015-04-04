/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VARIANTTUPLE_HPP
#define CPPWAMP_VARIANTTUPLE_HPP

//------------------------------------------------------------------------------
/** @file
    Provides operators allowing Variant to interact with std::tuple. */
//------------------------------------------------------------------------------

#include <tuple>
#include "variant.hpp"

namespace wamp
{

/// @name Non-member Tuple Operations (in varianttuple.hpp)
/// @{

//------------------------------------------------------------------------------
/** Converts a `std::tuple` to a Variant::Array.
    @pre The tuple values must be convertible to Variant's bound types
         (statically checked).
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
Array toArray(std::tuple<Ts...> tuple);

//------------------------------------------------------------------------------
/** Converts a Variant::Array to a `std::tuple`.
    @pre The Array's element types must be convertible to the
         tuple's element types.
    @throws error::Conversion if one of the Array element types is not
            convertible to the target type.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
void toTuple(const Array& array, std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Checks if a Variant::Array is convertible to a `std::tuple`.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool convertsToTuple(const Array& array, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a Variant::Array and a std::tuple for equality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const Array& array, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a std::tuple and a Variant::Array for equality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const std::tuple<Ts...>& tuple, const Array& array);

//------------------------------------------------------------------------------
/** Compares a Variant::Array and a std::tuple for inequality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const Array& array, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a std::tuple and a Variant::Array for inequality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const std::tuple<Ts...>& tuple, const Array& array);

//------------------------------------------------------------------------------
/** Compares a Variant and a std::tuple for equality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const Variant& variant, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a std::tuple and a Variant for equality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const std::tuple<Ts...>& tuple, const Variant& variant);

//------------------------------------------------------------------------------
/** Compares a Variant and a std::tuple for inequality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const Variant& variant, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a std::tuple and a Variant for inequality.
    @relates Variant */
//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const std::tuple<Ts...>& tuple, const Variant& variant);

/// @}


} // namespace wamp

#include "internal/varianttuple.ipp"

#endif // CPPWAMP_VARIANTTUPLE_HPP
