/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2017, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TYPES_ARRAY_HPP
#define CPPWAMP_TYPES_ARRAY_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities allowing Variant to interoperate
           with std::array. */
//------------------------------------------------------------------------------

#include <array>
#include "../api.hpp"
#include "../error.hpp"
#include "../variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Performs the conversion from an array variant to a `std::array`.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename T, std::size_t Size>
CPPWAMP_API void convert(FromVariantConverter& conv, std::array<T, Size>& array)
{
    using namespace wamp;
    const auto& variant = conv.variant();
    if (variant.is<Array>() == false)
    {
        throw error::Conversion("Attempting to convert non-array variant "
                                "to std::array");
    }

    std::array<T, Size> newArray;
    const auto& variantArray = variant.as<Array>();
    if (variantArray.size() != Size)
    {
        throw error::Conversion("Variant array size does not match that "
            "of std::array<T," + std::to_string(Size) + ">");
    }

    for (Array::size_type i=0; i<variantArray.size(); ++i)
    {
        try
        {
            newArray[i] = variantArray[i].to<T>();
        }
        catch (const error::Conversion& e)
        {
            std::string msg = e.what();
            msg += " (for element #" + std::to_string(i) + ")";
            throw error::Conversion(msg);
        }
    }
    array = std::move(newArray);
}

//------------------------------------------------------------------------------
/** Performs the conversion from a `std::array` to an array variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename T, std::size_t Size>
CPPWAMP_API void convert(ToVariantConverter& conv, std::array<T, Size>& array)
{
    using namespace wamp;
    Array variantArray;
    for (const auto& elem: array)
    {
        variantArray.emplace_back(Variant::from(elem));
    }
    conv.variant() = std::move(variantArray);
}

} // namespace wamp

#endif // CPPWAMP_TYPES_ARRAY_HPP
