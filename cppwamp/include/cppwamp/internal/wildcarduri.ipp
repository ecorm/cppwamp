/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2016, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../utils/wildcarduri.hpp"
#include "../api.hpp"
#include "../error.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOr<SplitUri::uri_type> SplitUri::flatten() const
{
    if (labels_.empty())
        return makeUnexpectedError(std::errc::result_out_of_range);

    std::string uri;
    bool first = true;
    for (const auto& label: labels_)
    {
        if (!first)
            uri += separator;
        first = false;
        uri += label;
    }
    return uri;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SplitUri::storage_type SplitUri::tokenize(const uri_type uri)
{
    storage_type tokens;
    if (uri.empty())
    {
        tokens.emplace_back();
        return tokens;
    }

    auto next = uri.cbegin();
    auto end = uri.cend();
    std::string::const_iterator iter;
    for (iter = uri.cbegin(); iter != end; ++iter)
    {
        if (*iter == separator)
        {
            tokens.emplace_back(next, iter);
            next = iter + 1;
        }
    }

    if (iter != next)
        tokens.emplace_back(next, iter);

    // If URI ends with a dot, add a wildcard token at the end.
    if (!uri.empty() && uri.back() == separator)
        tokens.emplace_back();

    return tokens;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SplitUri::uri_type
SplitUri::untokenize(const storage_type& labels)
{
    CPPWAMP_LOGIC_CHECK(!labels.empty(),
                        "wamp::untokenizeUri labels cannot be empty");

    static constexpr char separator = '.';
    std::string uri;
    bool first = true;
    for (auto& label: labels)
    {
        if (!first)
            uri += separator;
        first = false;
        uri += label;
    }
    return uri;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool matchesWildcardPattern(const SplitUri& uri,
                                           const SplitUri& pattern)
{
    auto uriSize = uri.size();
    if (uriSize != pattern.size())
        return false;
    for (SplitUri::size_type i = 0; i != uriSize; ++i)
        if (!isWildcardLabel(pattern[i]) && (uri[i] != pattern[i]))
            return false;
    return true;
}

} // namespace utils

} // namespace wamp
