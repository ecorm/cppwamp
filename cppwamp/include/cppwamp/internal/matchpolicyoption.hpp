/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_MATCHPOLICYOPTION_HPP
#define CPPWAMP_INTERNAL_MATCHPOLICYOPTION_HPP

#include <cassert>
#include "../exceptions.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline MatchPolicy parseMatchPolicy(const Variant& option)
{
    if (!option.is<String>())
        return MatchPolicy::unknown;

    const auto& matchStr = option.as<String>();
    if (matchStr.empty() || matchStr == "exact")
        return MatchPolicy::exact;
    if (matchStr == "prefix")
        return MatchPolicy::prefix;
    if (matchStr == "wildcard")
        return MatchPolicy::wildcard;
    return MatchPolicy::unknown;
}

//------------------------------------------------------------------------------
inline MatchPolicy getMatchPolicyOption(const Object& options)
{
    auto found = options.find("match");
    if (found == options.end())
        return MatchPolicy::exact;
    return parseMatchPolicy(found->second);
}

//------------------------------------------------------------------------------
inline String toString(const MatchPolicy& p)
{
    switch (p)
    {
    case MatchPolicy::exact:    return "exact";
    case MatchPolicy::prefix:   return "prefix";
    case MatchPolicy::wildcard: return "wildcard";
    default: break;
    }

    assert(false && "Unexpected MatchPolicy enumerator");
    return {};
}

//------------------------------------------------------------------------------
inline void setMatchPolicyOption(Object& options, MatchPolicy policy)
{
    CPPWAMP_LOGIC_CHECK(policy != MatchPolicy::unknown,
                        "Cannot specify unknown match policy");

    if (policy == MatchPolicy::exact)
        options.erase("match");
    else
        options["match"] = toString(policy);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_MATCHPOLICYOPTION_HPP
