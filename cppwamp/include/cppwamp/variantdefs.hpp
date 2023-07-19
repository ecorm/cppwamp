/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VARIANTDEFS_HPP
#define CPPWAMP_VARIANTDEFS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains fundamental type definitions related to Variant. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace wamp
{

//------------------------------------------------------------------------------
/** Integer ID used to indicate the current dynamic type of a `Variant`. */
//------------------------------------------------------------------------------
enum class TypeId : uint8_t // TODO: Rename to VariantKind
{
    null,       ///< For Variant::Null
    boolean,    ///< For Variant::Bool
    integer,    ///< For Variant::Int
    uint,       ///< For Variant::UInt
    real,       ///< For Variant::Real
    string,     ///< For Variant::String
    blob,       ///< For Variant::Blob
    array,      ///< For Variant::Array
    object      ///< For Variant::Object
};

//------------------------------------------------------------------------------
/** @name Non-composite Variant bound types */
//------------------------------------------------------------------------------
/// @{
using Bool   = bool;          ///< Variant bound type for boolean values
using Int    = std::int64_t;  ///< Variant bound type for signed integers
using UInt   = std::uint64_t; ///< Variant bound type for unsigned integers
using Real   = double;        ///< Variant bound type for floating-point numbers
using String = std::string;   ///< Variant bound type for text strings
/// @}

} // namespace wamp

#endif // CPPWAMP_VARIANTDEFS_HPP
