/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_URI_HPP
#define CPPWAMP_URI_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for validing URIs. */
//------------------------------------------------------------------------------

#include <functional>
#include <locale>
#include "variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** URI validator that follows the rules in the [protocol specification][1].
    [1]: https://wamp-proto.org/wamp_latest_ietf.html#name-uris
    @tparam TCharValidator Type used to determine if characters within
                           URI components are valid. */
//------------------------------------------------------------------------------
template <typename TCharValidator>
class BasicUriValidator
{
public:
    /// Validator type for characters within URI components.
    using CharValidator = TCharValidator;

    /** Default constructor. */
    BasicUriValidator();

    /** Determines if the given URI is valid. */
    bool operator()(
        const String& uri, /**< The URI to validate */
        bool isPattern     /**< True if the URI to validate is used for
                                pattern-based subscriptions/registrations. */
        ) const;

private:
    using Char = String::value_type;
    bool checkAsPattern(const Char* ptr, const Char* end) const;
    bool checkAsRessource(const Char* ptr, const Char* end) const;
    bool tokenIsValid(const Char* first, const Char* last) const;
    std::locale locale_;
};

//------------------------------------------------------------------------------
/** URI character validator that rejects the `#` or whitespace characters. */
//------------------------------------------------------------------------------
struct RelaxedUriCharValidator
{
    using Char = String::value_type;

    static bool isValid(Char c, const std::locale& loc)
    {
        return !std::isspace(c, loc) && (c != '#');
    }
};

//------------------------------------------------------------------------------
/** URI character validator that allows only lowercase letters, digits and
    underscore (`_`). */
//------------------------------------------------------------------------------
struct StrictUriCharValidator
{
    using Char = String::value_type;

    static bool isValid(Char c, const std::locale& loc)
    {
        return std::islower(c, loc) || (c == '_');
    }
};

//------------------------------------------------------------------------------
/** URI character validator that rejects the `#` or whitespace characters
    within URI components. */
//------------------------------------------------------------------------------
using RelaxedUriValidator = BasicUriValidator<RelaxedUriCharValidator>;

//------------------------------------------------------------------------------
/** URI character validator that allows only lowercase letters, digits and
    underscore (`_`) within URI components. */
//------------------------------------------------------------------------------
using StrictUriValidator = BasicUriValidator<StrictUriCharValidator>;

//------------------------------------------------------------------------------
/** Handler type for URI validation. */
//------------------------------------------------------------------------------
using UriValidator = std::function<bool (const String&, bool isPattern)>;


//******************************************************************************
// BasicUriValidator member function definitions
//******************************************************************************

template <typename V>
BasicUriValidator<V>::BasicUriValidator() : locale_("C") {}

template <typename V>
bool BasicUriValidator<V>::operator()(const String& uri, bool isPattern) const
{
    const Char* c = uri.data();
    const Char* end = c + uri.size();

    if (isPattern)
        return checkAsPattern(c, end);
    return checkAsRessource(c, end);
}

template <typename V>
bool BasicUriValidator<V>::checkAsPattern(const Char* ptr,
                                          const Char* end) const
{
    for (; ptr != end; ++ptr)
        if (*ptr != '.' && !CharValidator::isValid(*ptr, locale_))
            return false;
    return true;
}

template <typename V>
bool BasicUriValidator<V>::checkAsRessource(const Char* begin,
                                            const Char* end) const
{
    if (begin == end)
        return false;
    const Char* ptr = begin;
    const Char* tokenStart = begin;
    while (ptr != end)
    {
        if (*ptr == '.')
        {
            if (!tokenIsValid(tokenStart, ptr))
                return false;
            tokenStart = ++ptr;
        }
        else
        {
            ++ptr;
        }
    }
    return tokenIsValid(tokenStart, end);
}

template <typename V>
bool BasicUriValidator<V>::tokenIsValid(const Char* first,
                                        const Char* last) const
{
    if (first == last)
        return false;
    return checkAsPattern(first, last);
}

} // namespace wamp

#endif // CPPWAMP_URI_HPP
