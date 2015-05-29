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
    template <typename... Ts>
    TDerived& withArgs(Ts&&... args);

    /** Sets the positional arguments for this payload from
        an array of variants. */
    TDerived& withArgList(Array args);

    /** Sets the keyword arguments for this payload. */
    TDerived& withKwargs(Object kwargs);

    /** Accesses the constant list of positional arguments. */
    const Array& args() const &;

    /** Returns the moved list of positional arguments. */
    Array args() &&;

    /** Accesses the constant map of keyword arguments. */
    const Object& kwargs() const &;

    /** Returns the moved map of keyword arguments. */
    Object kwargs() &&;

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

} // namespace wamp

#include "internal/payload.ipp"

#endif // CPPWAMP_PAYLOAD_HPP
