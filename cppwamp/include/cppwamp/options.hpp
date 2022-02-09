/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_OPTIONS_HPP
#define CPPWAMP_OPTIONS_HPP

#include "traits.hpp"
#include "variant.hpp"
#include "./internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the Options class. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Represents a collection of WAMP options or details. */
//------------------------------------------------------------------------------
template <typename TDerived>
class Options
{
public:
    /** Adds an option. */
    TDerived& withOption(String key, Variant value);

    /** Sets all options at once. */
    TDerived& withOptions(Object options);

    /** Obtains the entire dictionary of options. */
    const Object& options() const;

    /** Obtains an option by key. */
    Variant optionByKey(const String& key) const;

    /** Obtains an option by key or a fallback value. */
    template <typename T>
    ValueTypeOf<T> optionOr(const String& key, T&& fallback) const;

protected:
    /** Default constructor. */
    Options();

    /** Constructor taking initial options. */
    explicit Options(Object options);

private:
    Object options_;

public:
    Object& options(internal::PassKey); // Internal use only
};

} // namespace wamp

#include "./internal/options.ipp"


#endif // CPPWAMP_OPTIONS_HPP
