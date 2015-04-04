/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ARGS_HPP
#define CPPWAMP_ARGS_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of Args, which bundles together Variant
    arguments. */
//------------------------------------------------------------------------------

#include <initializer_list>
#include <ostream>
#include "variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type used to distinguish Args constructor overloads.
    @see wamp::with */
//------------------------------------------------------------------------------
struct With { };

//------------------------------------------------------------------------------
/** Tag type used to distinguish Args constructor overloads.
    @see wamp::withPairs */
//------------------------------------------------------------------------------
struct WithPairs { };

//------------------------------------------------------------------------------
/** Constant tag object used to distinguish Args constructor overloads. */
//------------------------------------------------------------------------------
constexpr With with{};

//------------------------------------------------------------------------------
/** Constant tag object used to distinguish Args constructor overloads. */
//------------------------------------------------------------------------------
constexpr WithPairs withPairs{};

//------------------------------------------------------------------------------
/** Bundles variants into positional and/or keyword arguments.
    These arguments are exchanged with a WAMP peer via the Client APIs. */
//------------------------------------------------------------------------------
struct Args
{
    /** Braced initializer list of keyword/variant pairs. */
    using PairInitializerList =
        std::initializer_list<std::pair<const String, Variant>>;

    /** Default constructor */
    Args();

    /** Converting constructor taking a braced initializer list of positional
        variant arguments. */
    Args(std::initializer_list<Variant> positional);

    /** Constructor taking a braced initializer list of keyword/variant
        pairs. */
    Args(WithPairs withPairs, PairInitializerList pairs);

    /** Constructor taking a dynamic array of positional variant arguments. */
    Args(With with, Array list);

    /** Constructor taking a map of keyword arguments. */
    Args(With with, Object map);

    /** Constructor taking both positional and keyword arguments. */
    Args(With with, Array list, Object map);

    /** Converts the Args::list positional arguments to the given types. */
    template <typename... Ts> size_t to(Ts&... vars) const;

    /** Moves the Args::list positional arguments to the given value
        references. */
    template <typename... Ts> size_t move(Ts&... vars);

    /** Accesses a positional argument from Args::list by index. */
    Variant& operator[](size_t index);

    /** Accesses a constant positional argument from Args::list by index. */
    const Variant& operator[](size_t index) const;

    /** Accesses a keyword argument from Args::map. */
    Variant& operator[](const String& keyword);

    /** Compares two Args objects for equality. */
    bool operator==(const Args& rhs) const;

    /** Compares two Args objects for inequality. */
    bool operator!=(const Args& rhs) const;

    // These are public members because the library sometimes moves them
    // for efficiency reasons.
    Array list; ///< Dynamic array of positional arguments.
    Object map; ///< Map (dictionary) of keyword arguments.
};

//------------------------------------------------------------------------------
/** @relates Args
    Outputs an Args object to the given stream. */
//------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const Args& args);


//------------------------------------------------------------------------------
/** Utility class used to split @ref Array elements into separate arguments
    to be passed to a function.
    This class is used internally by Client to invoke @ref StaticEventSlot and
    @ref StaticCallSlot targets.
    @tparam TArgs List of argument types to which the Array elements will be
            split and converted. */
//------------------------------------------------------------------------------
template <typename... TArgs>
struct Unmarshall
{
    /** Calls the given function with the given array elements split up as
        distinct function arguments. */
    template <typename TFunction>
    static void apply(TFunction&& fn, const Array& array);

    /** Calls the given function with the given array elements split up as
        distinct function arguments. */
    template <typename TFunction, typename... TPreargs>
    static void apply(TFunction&& fn, const Array& array,
                      TPreargs&&... preargs);
};

} // namespace wamp

#include "internal/args.ipp"

#endif // CPPWAMP_ARGS_HPP
