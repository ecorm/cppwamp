/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_OPTIONS_HPP
#define CPPWAMP_OPTIONS_HPP

#include <utility>
#include "api.hpp"
#include "errorcodes.hpp"
#include "erroror.hpp"
#include "traits.hpp"
#include "variant.hpp"
#include "./internal/message.hpp"
#include "./internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities for accessing WAMP message options. */
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
template <typename TDerived, unsigned O>
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
    static constexpr unsigned optionsPos = O;

    Options(internal::MessageKind kind, Array&& fields);

    explicit Options(internal::Message&& msg);

    internal::Message& message();

    const internal::Message& message() const;

private:
    internal::Message message_;

public:
    // Internal use only
    internal::Message& message(internal::PassKey);
};


//------------------------------------------------------------------------------
template <typename D, unsigned O>
D& Options<D,O>::withOption(String key, Variant value)
{
    options().emplace(std::move(key), std::move(value));
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
D& Options<D,O>::withOptions(Object opts)
{
    options() = std::move(opts);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
const Object& Options<D,O>::options() const &
{
    return message_.at(optionsPos).as<Object>();
}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
Object&& Options<D,O>::options() &&
{
    return std::move(message_.at(optionsPos).as<Object>());
}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
const Variant& Options<D,O>::optionByKey(const String& key) const
{
    static const Variant nullVariant;
    auto iter = options().find(key);
    if (iter != options().end())
        return iter->second;
    return nullVariant;
}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
template <typename T, typename U>
T Options<D,O>::optionOr(
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
             Errc::absent or Errc::badType. */
//------------------------------------------------------------------------------
template <typename D, unsigned O>
template <typename T>
ErrorOr<T> Options<D,O>::optionAs(
    const String& key /**< The key to search under. */
    ) const
{
    auto iter = options().find(key);
    if (iter == options().end())
        return makeUnexpectedError(Errc::absent);
    if (!iter->second.template is<T>())
        return makeUnexpectedError(Errc::badType);
    return iter->second.template as<T>();
}

//------------------------------------------------------------------------------
/** @returns The option value, or an error code of either
    WampErrc::no_such_option or WampErrc::bad_option. */
//------------------------------------------------------------------------------
template <typename D, unsigned O>
ErrorOr<UInt> Options<D,O>::toUnsignedInteger(const String& key) const
{
    auto found = options().find(key);
    if (found == options().end())
        return makeUnexpectedError(Errc::absent);
    const auto& v = found->second;
    UInt n;
    if (!optionToUnsignedInteger(v, n))
        return makeUnexpectedError(Errc::badType);
    return n;
}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
Options<D,O>::Options(internal::MessageKind kind, Array&& fields)
    : message_(kind, std::move(fields))
{}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
Options<D,O>::Options(internal::Message&& msg)
    : message_(std::move(msg))
{}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
internal::Message& Options<D,O>::message() {return message_;}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
const internal::Message& Options<D,O>::message() const {return message_;}

//------------------------------------------------------------------------------
template <typename D, unsigned O>
internal::Message& Options<D,O>::message(internal::PassKey) {return message_;}

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
