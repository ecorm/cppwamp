/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TYPES_OPTIONAL_HPP
#define CPPWAMP_TYPES_OPTIONAL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities allowing Variant to interoperate with
           std::optional. */
//------------------------------------------------------------------------------

#include <optional>
#include "../api.hpp"
#include "../variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Performs the conversion from a variant to a std::optional.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API void convert(FromVariantConverter& conv, std::optional<T>& opt)
{
    const auto& variant = conv.variant();
    if (!variant)
        opt.reset();
    else
        opt = variant.to<T>();
}

//------------------------------------------------------------------------------
/** Performs the conversion from a std::optional to a variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API void convert(ToVariantConverter& conv, std::optional<T>& opt)
{
    auto& variant = conv.variant();
    if (!opt)
        variant = null;
    else
        variant = Variant::from(*opt);
}


//------------------------------------------------------------------------------
// Comparison Operators
//------------------------------------------------------------------------------

/// Compares a variant and a `std::optional` for equality.
template <typename T>
CPPWAMP_API bool operator==(const Variant& v, const std::optional<T> o)
{
    return !o ? !v : (v == *o);
}

/// Compares a variant and a `std::optional` for equality.
template <typename T>
CPPWAMP_API bool operator==(const std::optional<T> o, const Variant& v)
{
    return v == o;
}

/// Compares a variant and a `std::optional` for inequality.
template <typename T>
CPPWAMP_API bool operator!=(const Variant& v, const std::optional<T> o)
{
    return !o ? !!v : (v != *o);
}

/// Compares a variant and a `std::optional` for inequality.
template <typename T>
CPPWAMP_API bool operator!=(const std::optional<T> o, const Variant& v)
{
    return v != o;
}

} // namespace wamp

#endif // CPPWAMP_TYPES_OPTIONAL_HPP
