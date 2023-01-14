/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2016, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../uri.hpp"
#include "../api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE SplitUri tokenizeUri(const std::string uri)
{
    static constexpr char separator = '.';
    SplitUri tokens;

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
CPPWAMP_INLINE std::string untokenizeUri(const SplitUri& labels)
{
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
CPPWAMP_INLINE bool uriMatchesWildcardPattern(const SplitUri& uri,
                                              const SplitUri& pattern)
{
    auto uriSize = uri.size();
    if (uriSize != pattern.size())
        return false;
    for (SplitUri::size_type i = 0; i != uriSize; ++i)
        if (!pattern[i].empty() && uri[i] != pattern[i])
            return false;
    return true;
}

} // namespace wamp
