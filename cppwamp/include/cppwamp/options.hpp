/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_OPTIONS_HPP
#define CPPWAMP_OPTIONS_HPP

#include <utility>
#include "api.hpp"
#include "traits.hpp"
#include "variant.hpp"
#include "./internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Options class. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Represents a collection of WAMP options or details. */
//------------------------------------------------------------------------------
template <typename TDerived>
class CPPWAMP_API Options
{
public:
    /** Adds an option. */
    TDerived& withOption(String key, Variant value)
    {
        options_.emplace(std::move(key), std::move(value));
        return static_cast<TDerived&>(*this);
    }

    /** Sets all options at once. */
    TDerived& withOptions(Object options)
    {
        options_ = std::move(options);
        return static_cast<TDerived&>(*this);
    }

    /** Obtains the entire dictionary of options. */
    const Object& options() const {return options_;}

    /** Obtains an option by key. */
    Variant optionByKey(const String& key) const
    {
        Variant result;
        auto iter = options_.find(key);
        if (iter != options_.end())
            result = iter->second;
        return result;
    }

    /** Obtains an option by key or a fallback value. */
    template <typename T>
    ValueTypeOf<T> optionOr(
        const String& key, /**< The key to search under. */
        T&& fallback       /**< The fallback value to return if the key was
                                not found. */
        ) const
    {
        auto iter = options_.find(key);
        if (iter != options_.end())
            return iter->second.template to<ValueTypeOf<T>>();
        else
            return std::forward<T>(fallback);
    }

protected:
    /** Default constructor. */
    Options() = default;

    /** Constructor taking initial options. */
    explicit Options(Object options) : options_(std::move(options)) {}

private:
    Object options_;

public:
    CPPWAMP_HIDDEN Object& options(internal::PassKey) // Internal use only
    {
        return options_;
    }
};

} // namespace wamp

#endif // CPPWAMP_OPTIONS_HPP
