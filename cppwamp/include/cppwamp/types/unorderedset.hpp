/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TYPES_UNORDEREDSET_HPP
#define CPPWAMP_TYPES_UNORDEREDSET_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities allowing Variant to interoperate
           with std::unordered_set. */
//------------------------------------------------------------------------------

#include <unordered_set>
#include "../api.hpp"
#include "../error.hpp"
#include "../variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Performs the conversion from an array variant to a `std::unordered_set`.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API void convert(FromVariantConverter& conv, std::unordered_set<T>& set)
{
    const auto& variant = conv.variant();
    if (!variant.is<Array>())
    {
        throw error::Conversion("Attempting to convert non-array variant "
                                "to std::unordered_set");
    }

    std::unordered_set<T> newSet;
    const auto& array = variant.as<Array>();
    for (Array::size_type i=0; i<array.size(); ++i)
    {
        try
        {
            newSet.emplace(array[i].to<T>());
        }
        catch (const error::Conversion& e)
        {
            std::string msg = e.what();
            msg += " (for element #" + std::to_string(i) + ")";
            throw error::Conversion(msg);
        }
    }
    set = std::move(newSet);
}

//------------------------------------------------------------------------------
/** Performs the conversion from a `std::unordered_set` to an array variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API void convert(ToVariantConverter& conv, std::unordered_set<T>& set)
{
    Array array;
    for (const auto& elem: set)
    {
        array.emplace_back(Variant::from(elem));
    }
    conv.variant() = std::move(array);
}

} // namespace wamp

#endif // CPPWAMP_TYPES_UNORDEREDSET_HPP
