/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PAYLOAD_HPP
#define CPPWAMP_PAYLOAD_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of Payload, which bundles together Variant
    arguments. */
//------------------------------------------------------------------------------

#include <initializer_list>
#include <ostream>
#include "variant.hpp"
#include "./internal/config.hpp"
#include "./internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a payload of positional and keyword arguments exchanged with a
    WAMP peer. */
//------------------------------------------------------------------------------
template <typename TDerived>
class Payload
{
public:
    /** Converting constructor taking a braced initializer list of positional
        variant arguments. */
    Payload(std::initializer_list<Variant> list);

    /** Sets the positional arguments for this payload. */
    TDerived& withArgs(Array args);

    /** Sets the keyword arguments for this payload. */
    TDerived& withKwargs(Object kwargs);

#if CPPWAMP_HAS_REF_QUALIFIERS
    /** Accesses the constant list of positional arguments. */
    const Array& args() const &;
#else
    /** Accesses the constant list of positional arguments. */
    const Array& args() const;
#endif

#if CPPWAMP_HAS_REF_QUALIFIERS
    /** Returns the moved list of positional arguments. */
    Array args() &&;
#endif

    /** Returns the moved list of positional arguments. */
    Array moveArgs();

    /** Accesses the constant map of keyword arguments. */
    const Object& kwargs() const &;

    /** Returns the moved map of keyword arguments. */
    Object kwargs() &&;

    /** Returns the moved map of keyword arguments. */
    Object moveKwargs();

    /** Accesses a positional argument by index. */
    Variant& operator[](size_t index);

    /** Accesses a constant positional argument by index. */
    const Variant& operator[](size_t index) const;

    /** Accesses a keyword argument by key. */
    Variant& operator[](const String& keyword);

    /** Converts the payload's positional arguments to the given value types. */
    template <typename... Ts> size_t convertTo(Ts&... values) const;

    /** Moves the payload's positional arguments to the given value
        references. */
    template <typename... Ts> size_t moveTo(Ts&... values);

protected:
    Payload();

private:
    Array args_;    // List of positional arguments.
    Object kwargs_; // Dictionary of keyword arguments.

public:
    // Internal use only
    Array& args(internal::PassKey);
    Object& kwargs(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Utility class used to split @ref Array elements into separate arguments
    to be passed to a function.
    This class is used internally by Session to invoke event slots and call
    slots taking extra, statically-typed parameters.
    @tparam TArgs List of target argument types to which the Array elements
            will be split and converted. */
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

#include "internal/payload.ipp"

#endif // CPPWAMP_PAYLOAD_HPP
