/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TYPES_BOOSTOPTIONAL_HPP
#define CPPWAMP_TYPES_BOOSTOPTIONAL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities allowing Variant to interoperate with
           boost::optional. */
//------------------------------------------------------------------------------

#include <boost/optional.hpp>
#include "../api.hpp"
#include "../variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Performs the conversion from a variant to a boost::optional.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API void convert(FromVariantConverter& conv, boost::optional<T>& opt)
{
    const auto& variant = conv.variant();
    if (!variant)
        opt.reset();
    else
        opt = variant.to<T>();
}

//------------------------------------------------------------------------------
/** Performs the conversion from a boost::optional to a variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_API void convert(ToVariantConverter& conv, boost::optional<T>& opt)
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

/// Compares a variant and a `boost::optional` for equality.
template <typename T>
CPPWAMP_API bool operator==(const Variant& v, const boost::optional<T> o)
{
    return !o ? !v : (v == *o);
}

/// Compares a variant and a `boost::optional` for equality.
template <typename T>
CPPWAMP_API bool operator==(const boost::optional<T> o, const Variant& v)
{
    return v == o;
}

/// Compares a variant and a `boost::optional` for inequality.
template <typename T>
CPPWAMP_API bool operator!=(const Variant& v, const boost::optional<T> o)
{
    return !o ? !!v : (v != *o);
}

/// Compares a variant and a `boost::optional` for inequality.
template <typename T>
CPPWAMP_API bool operator!=(const boost::optional<T> o, const Variant& v)
{
    return v != o;
}

} // namespace wamp

#endif // CPPWAMP_TYPES_BOOSTOPTIONAL_HPP
