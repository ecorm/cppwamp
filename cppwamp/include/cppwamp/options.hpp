/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_OPTIONS_HPP
#define CPPWAMP_OPTIONS_HPP

#include <utility>
#include "api.hpp"
#include "erroror.hpp"
#include "traits.hpp"
#include "variant.hpp"
#include "./internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for accessing WAMP message options. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Converts an option to an unsigned integer. */
//------------------------------------------------------------------------------
bool CPPWAMP_API optionToUnsignedInteger(const Variant& option, UInt& number);


//------------------------------------------------------------------------------
/** Wrapper around a WAMP message containing an options dictionary. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TMessage>
class CPPWAMP_API Options
{
public:
    /** Adds an option. */
    TDerived& withOption(String key, Variant value);

    /** Sets all options at once. */
    TDerived& withOptions(Object opts);

    /** Accesses the entire dictionary of options. */
    const Object& options() const &;

    /** Moves the entire dictionary of options. */
    Object&& options() &&;

    /** Obtains an option by key. */
    const Variant& optionByKey(const String& key) const;

    /** Obtains an option by key, converted to the given type, or a
        fallback value. */
    template <typename T, typename U>
    T optionOr(const String& key, U&& fallback) const;

    /** Obtains an option by key having the given type. */
    template <typename T>
    ErrorOr<T> optionAs(const String& key) const;

    /** Obtains an option by key, converted to an unsigned integer. */
    ErrorOr<UInt> toUnsignedInteger(const String& key) const;

protected:
    using MessageType = TMessage;

    /** Constructor taking message construction aruments. */
    template <typename... TArgs>
    explicit Options(TArgs&&... args);

    /** Accesses the underlying message containing these options. */
    MessageType& message();

    /** Accesses the underlying message containing these options. */
    const MessageType& message() const;

private:
    MessageType message_;

public:
    // Internal use only
    MessageType& message(internal::PassKey);
};


//------------------------------------------------------------------------------
template <typename D, typename M>
D& Options<D,M>::withOption(String key, Variant value)
{
    message_.options().emplace(std::move(key), std::move(value));
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, typename M>
D& Options<D,M>::withOptions(Object opts)
{
    message_.options() = std::move(opts);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, typename M>
const Object& Options<D,M>::options() const & {return message_.options();}

//------------------------------------------------------------------------------
template <typename D, typename M>
Object&& Options<D,M>::options() && {return std::move(message_).options();}

//------------------------------------------------------------------------------
template <typename D, typename M>
const Variant& Options<D,M>::optionByKey(const String& key) const
{
    static const Variant nullVariant;
    auto iter = options().find(key);
    if (iter != options().end())
        return iter->second;
    return nullVariant;
}

//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename T, typename U>
T Options<D,M>::optionOr(
    const String& key, /**< The key to search under. */
    U&& fallback       /**< The fallback value to return if the key was
                                not found or cannot be converted. */
    ) const
{
    auto iter = options().find(key);
    if (iter == options().end())
        return std::forward<U>(fallback);

    try
    {
        return iter->second.template to<ValueTypeOf<T>>();
    }
    catch (const error::Conversion&)
    {
        return std::forward<U>(fallback);
    }
}

//------------------------------------------------------------------------------
/** @returns The option value, or an error code of either
             WampErrc::no_such_option or WampErrc::bad_option. */
//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename T>
ErrorOr<T> Options<D,M>::optionAs(
    const String& key /**< The key to search under. */
    ) const
{
    auto iter = options().find(key);
    if (iter == options().end())
        return makeUnexpectedError(WampErrc::noSuchOption);
    if (!iter->second.template is<T>())
        return makeUnexpectedError(WampErrc::badOption);
    return iter->second.template as<T>();
}

//------------------------------------------------------------------------------
/** @returns The option value, or an error code of either
    WampErrc::no_such_option or WampErrc::bad_option. */
//------------------------------------------------------------------------------
template <typename D, typename M>
ErrorOr<UInt> Options<D,M>::toUnsignedInteger(const String& key) const
{
    auto found = options().find(key);
    if (found == options().end())
        return makeUnexpectedError(WampErrc::noSuchOption);
    const auto& v = found->second;
    UInt n;
    if (!optionToUnsignedInteger(v, n))
        return makeUnexpectedError(WampErrc::badOption);
    return n;
}

//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename... TArgs>
Options<D,M>::Options(TArgs&&... args)
    : message_(std::forward<TArgs>(args)...)
{}

//------------------------------------------------------------------------------
template <typename D, typename M>
M& Options<D,M>::message() {return message_;}

//------------------------------------------------------------------------------
template <typename D, typename M>
const M& Options<D,M>::message() const {return message_;}

//------------------------------------------------------------------------------
template <typename D, typename M>
M& Options<D,M>::message(internal::PassKey) {return message_;}

//------------------------------------------------------------------------------
/** @returns false if the option cannot be converted losslessly to an
             unsigned integer. */
//------------------------------------------------------------------------------
inline bool optionToUnsignedInteger(const Variant& option, UInt& number)
{
    switch (option.typeId())
    {
    case TypeId::integer:
    {
        auto n = option.as<Int>();
        if (n < 0)
            return false;
        number = n;
        break;
    }

    case TypeId::uint:
        number = option.as<UInt>();
        break;

    case TypeId::real:
    {
        auto x = option.as<Real>();
        if (x < 0)
            return false;

        auto n = static_cast<UInt>(x);
        // Round-trip back to floating point and check that it's still
        // equal to the original value.
        if (static_cast<Real>(n) != x)
            return false;
        number = n;
        break;
    }

    default:
        return false;
    }

    return true;
}

} // namespace wamp

#endif // CPPWAMP_OPTIONS_HPP
