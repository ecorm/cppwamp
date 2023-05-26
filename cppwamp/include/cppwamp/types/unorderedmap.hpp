/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TYPES_UNORDEREDMAP_HPP
#define CPPWAMP_TYPES_UNORDEREDMAP_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities allowing Variant to interoperate with
           std::unordered_map. */
//------------------------------------------------------------------------------

#include <unordered_map>
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"
#include "../variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Performs the conversion from an object variant to a `std::unordered_map`.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename T>
void CPPWAMP_API convert(FromVariantConverter& conv,
                         std::unordered_map<String, T>& map)
{
    const auto& variant = conv.variant();
    if (!variant.is<Object>())
    {
        throw error::Conversion("Attempting to convert non-object variant "
                                "to std::unordered_map");
    }

    std::unordered_map<String, T> newMap;
    for (const auto& kv: variant.as<Object>())
    {
        try
        {
            newMap.emplace(kv.first, kv.second.to<T>());
        }
        catch (const error::Conversion& e)
        {
            std::string msg = e.what();
            msg += " (for variant member '" + kv.first + "')";
            throw error::Conversion(msg);
        }
    }
    map = std::move(newMap);
}

//------------------------------------------------------------------------------
/** Performs the conversion from a `std::unordered_map` to an object variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename T>
void CPPWAMP_API convert(ToVariantConverter& conv,
                         std::unordered_map<String, T>& map)
{
    Object obj;
    for (const auto& kv: map)
    {
        obj.emplace(kv.first, Variant::from(kv.second));
    }
    conv.variant() = std::move(obj);
}


}

#endif // CPPWAMP_TYPES_UNORDEREDMAP_HPP
