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

#include <locale>
#include <memory>
#include "wampdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Abstract base class for user-defined URI validators. */
//------------------------------------------------------------------------------
class UriValidator
{
public:
    using Ptr = std::shared_ptr<UriValidator>;

    /** Destructor. */
    virtual ~UriValidator() = default;

    /** Validates the given topic URI. */
    bool checkTopic(const Uri& uri, bool isPattern) const;

    /** Validates the given procedure URI. */
    bool checkProcedure(const Uri& uri, bool isPattern) const;

    /** Validates the given error URI. */
    bool checkError(const Uri& uri) const;

protected:
    /** Must be overriden to check the given topic URI. */
    virtual bool validateTopic(const Uri&) const = 0;

    /** Must be overriden to check  the given topic pattern URI. */
    virtual bool validateTopicPattern(const Uri&) const = 0;

    /** Must be overriden to check the given procedure URI. */
    virtual bool validateProcedure(const Uri&) const = 0;

    /** Must be overriden to check the given procedure pattern URI. */
    virtual bool validateProcedurePattern(const Uri&) const = 0;

    /** Must be overriden to check the given error URI. */
    virtual bool validateError(const Uri&) const = 0;
};


//------------------------------------------------------------------------------
/** URI validator that follows the rules in the [protocol specification][1].
    [1]: https://wamp-proto.org/wamp_latest_ietf.html#name-uris
    @tparam TCharValidator Type used to determine if characters within
                           URI components are valid. */
//------------------------------------------------------------------------------
template <typename TCharValidator>
class BasicUriValidator : public UriValidator
{
public:
    /// Validator type for characters within URI components.
    using CharValidator = TCharValidator;

    /** Creates an instance of the validator. */
    static Ptr create();

    ~BasicUriValidator() override = default;

    /** @name Non-copyable and non-movable */
    /// @{
    BasicUriValidator(const BasicUriValidator&) = delete;
    BasicUriValidator(BasicUriValidator&&) = delete;
    BasicUriValidator& operator=(const BasicUriValidator&) = delete;
    BasicUriValidator& operator=(BasicUriValidator&&) = delete;
    /// @}

protected:
    bool validateTopic(const Uri& uri) const override;

    bool validateTopicPattern(const Uri& uri) const override;

    bool validateProcedure(const Uri& uri) const override;

    bool validateProcedurePattern(const Uri& uri) const override;

    bool validateError(const Uri& uri) const override;

private:
    BasicUriValidator();

    using Char = Uri::value_type;
    bool checkAsRessource(const Char* ptr, const Char* end) const;
    bool checkAsPattern(const Char* ptr, const Char* end) const;
    bool tokenIsValid(const Char* first, const Char* last) const;
    std::locale locale_;
};

//------------------------------------------------------------------------------
/** URI character validator that rejects the `#` or whitespace characters. */
//------------------------------------------------------------------------------
struct RelaxedUriCharValidator
{
    using Char = Uri::value_type;

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
    using Char = Uri::value_type;

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


//******************************************************************************
// UriValidator member function definitions
//******************************************************************************

inline bool UriValidator::checkTopic(
    const Uri& uri, /**< The URI to validate */
    bool isPattern  /**< True if the URI to validate is used for
                             pattern-based subscriptions/registrations. */
    ) const
{
    return isPattern ? validateTopicPattern(uri)
                     : validateTopic(uri);
}

inline bool UriValidator::checkProcedure(
    const Uri& uri, /**< The URI to validate */
    bool isPattern  /**< True if the URI to validate is used for
                             pattern-based subscriptions/registrations. */
    ) const
{
    return isPattern ? validateProcedurePattern(uri)
                     : validateProcedure(uri);
}

inline bool UriValidator::checkError(const Uri& uri) const
{
    return validateError(uri);
}


//******************************************************************************
// BasicUriValidator member function definitions
//******************************************************************************

template<typename TCharValidator>
UriValidator::Ptr BasicUriValidator<TCharValidator>::create()
{
    return Ptr(new BasicUriValidator);
}

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
template <typename V>
bool BasicUriValidator<V>::validateTopic(const Uri& uri) const
{
    return checkAsRessource(uri.data(), uri.data() + uri.size());
}

template <typename V>
bool BasicUriValidator<V>::validateTopicPattern(const Uri& uri) const
{
    return checkAsPattern(uri.data(), uri.data() + uri.size());
}

template <typename V>
bool BasicUriValidator<V>::validateProcedure(const Uri& uri) const
{
    return checkAsRessource(uri.data(), uri.data() + uri.size());
}

template <typename V>
bool BasicUriValidator<V>::validateProcedurePattern(const Uri& uri) const
{
    return checkAsPattern(uri.data(), uri.data() + uri.size());
}

template <typename V>
bool BasicUriValidator<V>::validateError(const Uri& uri) const
{
    return checkAsRessource(uri.data(), uri.data() + uri.size());
}
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

template <typename V>
BasicUriValidator<V>::BasicUriValidator() : locale_("C") {}

template <typename V>
bool BasicUriValidator<V>::checkAsRessource(const Char* ptr,
                                            const Char* end) const
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (ptr == end)
        return false;

    const Char* tokenStart = ptr;
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
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

template <typename V>
bool BasicUriValidator<V>::checkAsPattern(const Char* ptr,
                                          const Char* end) const
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (; ptr != end; ++ptr)
        if (*ptr != '.' && !CharValidator::isValid(*ptr, locale_))
            return false;
    return true;
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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
