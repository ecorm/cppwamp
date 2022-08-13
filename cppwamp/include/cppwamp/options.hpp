/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_OPTIONS_HPP
#define CPPWAMP_OPTIONS_HPP

#include <utility>
#include "api.hpp"
#include "traits.hpp"
#include "variant.hpp"
#include "./internal/passkey.hpp"
#include "./internal/wampmessage.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Options class. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Wrapper around a WAMP message containing an options dictionary. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TMessage>
class CPPWAMP_API Options
{
public:
    /** Adds an option. */
    TDerived& withOption(String key, Variant value)
    {
        message_.options().emplace(std::move(key), std::move(value));
        return static_cast<TDerived&>(*this);
    }

    /** Sets all options at once. */
    TDerived& withOptions(Object opts)
    {
        message_.options() = std::move(opts);
        return static_cast<TDerived&>(*this);
    }

    /** Accesses the entire dictionary of options. */
    const Object& options() const {return message_.options();}

    /** Obtains an option by key. */
    Variant optionByKey(const String& key) const
    {
        Variant result;
        auto iter = options().find(key);
        if (iter != options().end())
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
        auto iter = options().find(key);
        if (iter != options().end())
            return iter->second.template to<ValueTypeOf<T>>();
        else
            return std::forward<T>(fallback);
    }

protected:
    using MessageType = TMessage;

    /** Constructor taking message construction aruments. */
    template <typename... TArgs>
    explicit Options(TArgs&&... args)
        : message_(std::forward<TArgs>(args)...)
    {}

    MessageType& message() {return message_;}

    const MessageType& message() const {return message_;}

private:
    MessageType message_;

public:
    // Internal use only
    MessageType& message(internal::PassKey) {return message_;}
};

} // namespace wamp

#endif // CPPWAMP_OPTIONS_HPP
