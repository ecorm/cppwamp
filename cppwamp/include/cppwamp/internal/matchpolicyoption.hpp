/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_MATCHPOLICYOPTION_HPP
#define CPPWAMP_INTERNAL_MATCHPOLICYOPTION_HPP

#include "../error.hpp"
#include "../wampdefs.hpp"
#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename T>
MatchPolicy getMatchPolicyOption(const T& messageData)
{
    const auto& opts = messageData.options();
    auto found = opts.find("match");
    if (found == opts.end())
        return MatchPolicy::exact;
    const auto& opt = found->second;
    if (opt.template is<String>())
    {
        const auto& s = opt.template as<String>();
        if (s == "prefix")
            return MatchPolicy::prefix;
        if (s == "wildcard")
            return MatchPolicy::wildcard;
    }
    return MatchPolicy::unknown;
}

//------------------------------------------------------------------------------
template <typename T>
void setMatchPolicyOption(T& messageData, MatchPolicy policy)
{
    CPPWAMP_LOGIC_CHECK(policy != MatchPolicy::unknown,
                        "Cannot specify unknown match policy");

    switch (policy)
    {
    case MatchPolicy::exact:
        break;

    case MatchPolicy::prefix:
        messageData.withOption("match", "prefix");
        break;

    case MatchPolicy::wildcard:
        messageData.withOption("match", "wildcard");
        break;

    default:
        assert(false && "Unexpected MatchPolicy enumerator");
    }
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_MATCHPOLICYOPTION_HPP
