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
#include "internal/message.hpp"

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
template <typename TDerived, internal::MessageKind K>
class Options : public internal::Command<K>
{
public:
    /** Adds an option. */
    TDerived& withOption(String key, Variant value);

    /** Sets all options at once. */
    TDerived& withOptions(Object opts);

    /** Accesses the entire dictionary of options. */
    const Object& options() const &;

    /** Accesses the entire dictionary of options. */
    Object& options() &;

    /** Moves the entire dictionary of options. */
    Object&& options() &&;

    /** Determines if an option is already set. */
    bool hasOption(const String& key) const;

    /** Obtains an option by key. */
    const Variant& optionByKey(const String& key) const;

    /** Obtains an option by key, converted to the given type, or a
        fallback value. */
    template <typename T, typename U>
    T optionOr(const String& key, U&& fallback) const;

    /** Obtains an option by key having the given type. */
    template <typename T>
    ErrorOr<T> optionAs(const String& key) const &;

    /** Moves an option by key having the given type. */
    template <typename T>
    ErrorOr<T> optionAs(const String& key) &&;

    /** Obtains an option by key, converted to an unsigned integer. */
    ErrorOr<UInt> toUnsignedInteger(const String& key) const;

protected:
    template <typename... Ts>
    Options(in_place_t, Ts&&... fields);

    template <internal::MessageKind M>
    explicit Options(internal::Command<M>&& command);

    explicit Options(internal::Message&& msg);

private:
    using Base = internal::Command<K>;

    static constexpr unsigned optionsPos_ =
        internal::MessageKindTraits<K>::optionsPos();
};


//------------------------------------------------------------------------------
/** @pre `this->hasOption(key) == false`
    @throws error::Logic if the precondition is not met. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
D& Options<D,K>::withOption(String key, Variant value)
{
    auto emplaced = options().emplace(std::move(key), value);
    CPPWAMP_LOGIC_CHECK(emplaced.second,
                        "wamp::Options::withOption: Option already exists");
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
D& Options<D,K>::withOptions(Object opts)
{
    options() = std::move(opts);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
const Object& Options<D,K>::options() const &
{
    return this->message().template as<Object>(optionsPos_);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Object& Options<D,K>::options() &
{
    return this->message().template as<Object>(optionsPos_);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Object&& Options<D,K>::options() &&
{
    return std::move(this->message().template as<Object>(optionsPos_));
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
bool Options<D,K>::hasOption(const String& key) const
{
    const auto& opts = options();
    return opts.find(key) != opts.end();
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
const Variant& Options<D,K>::optionByKey(const String& key) const
{
    static const Variant nullVariant;
    auto iter = options().find(key);
    if (iter != options().end())
        return iter->second;
    return nullVariant;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T, typename U>
T Options<D,K>::optionOr(
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
             MiscErrc::absent or MiscErrc::badType. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T>
ErrorOr<T> Options<D,K>::optionAs(
    const String& key /**< The key to search under. */
    ) const &
{
    auto iter = options().find(key);
    if (iter == options().end())
        return makeUnexpectedError(MiscErrc::absent);
    if (!iter->second.template is<T>())
        return makeUnexpectedError(MiscErrc::badType);
    return iter->second.template as<T>();
}

//------------------------------------------------------------------------------
/** @returns The option value, or an error code of either
             MiscErrc::absent or MiscErrc::badType. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T>
ErrorOr<T> Options<D,K>::optionAs(
    const String& key /**< The key to search under. */
    ) &&
{
    auto iter = options().find(key);
    if (iter == options().end())
        return makeUnexpectedError(MiscErrc::absent);
    if (!iter->second.template is<T>())
        return makeUnexpectedError(MiscErrc::badType);
    return std::move(iter->second.template as<T>());
}

//------------------------------------------------------------------------------
/** @returns The option value, or an error code of either
             MiscErrc::absent or MiscErrc::badType. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
ErrorOr<UInt> Options<D,K>::toUnsignedInteger(const String& key) const
{
    auto found = options().find(key);
    if (found == options().end())
        return makeUnexpectedError(MiscErrc::absent);
    const auto& v = found->second;
    UInt n;
    if (!optionToUnsignedInteger(v, n))
        return makeUnexpectedError(MiscErrc::badType);
    return n;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename... Ts>
Options<D,K>::Options(in_place_t, Ts&&... fields)
    : Base(in_place, std::forward<Ts>(fields)...)
{}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <internal::MessageKind M>
Options<D,K>::Options(internal::Command<M>&& command)
    : Base(std::move(command))
{}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Options<D,K>::Options(internal::Message&& msg)
    : Base(std::move(msg))
{}


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
