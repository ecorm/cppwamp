/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TYPES_SET_HPP
#define CPPWAMP_TYPES_SET_HPP

//------------------------------------------------------------------------------
/** @file
    Provides facilities allowing Variant to interoperate with std::set. */
//------------------------------------------------------------------------------

#include <set>
#include "../conversion.hpp"
#include "../error.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Performs the conversion from an array variant to a `std::set`.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename T>
void convert(FromVariantConverter& conv, std::set<T>& set)
{
    const auto& variant = conv.variant();
    if (!variant.is<Array>())
    {
        throw error::Conversion("Attempting to convert non-array variant "
                                "to std::set");
    }

    std::set<T> newSet;
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
/** Performs the conversion from a `std::set` to an array variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename T>
void convert(ToVariantConverter& conv, std::set<T>& set)
{
    Array array;
    for (const auto& elem: set)
    {
        array.emplace_back(Variant::from(elem));
    }
    conv.variant() = std::move(array);
}

} // namespace wamp

#endif // CPPWAMP_TYPES_SET_HPP
