/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_INVOCATIONPOLICYOPTION_HPP
#define CPPWAMP_INTERNAL_INVOCATIONPOLICYOPTION_HPP

#include <cassert>
#include "../exceptions.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline InvocationPolicy parseInvocationPolicy(const Variant& option)
{
    if (!option.is<String>())
        return InvocationPolicy::unknown;

    const auto& invocationStr = option.as<String>();
    if (invocationStr.empty() || invocationStr == "single")
        return InvocationPolicy::single;
    if (invocationStr == "roundrobin")
        return InvocationPolicy::roundRobin;
    if (invocationStr == "random")
        return InvocationPolicy::random;
    if (invocationStr == "first")
        return InvocationPolicy::first;
    if (invocationStr == "last")
        return InvocationPolicy::last;
    return InvocationPolicy::unknown;
}

//------------------------------------------------------------------------------
inline InvocationPolicy getInvocationPolicyOption(const Object& options)
{
    auto found = options.find("invoke");
    if (found == options.end())
        return InvocationPolicy::single;
    return parseInvocationPolicy(found->second);
}

//------------------------------------------------------------------------------
inline String toString(const InvocationPolicy& p)
{
    switch (p)
    {
    case InvocationPolicy::single:     return "single";
    case InvocationPolicy::roundRobin: return "roundrobin";
    case InvocationPolicy::random:     return "random";
    case InvocationPolicy::first:      return "first";
    case InvocationPolicy::last:       return "last";
    default: break;
    }

    assert(false && "Unexpected InvocationPolicy enumerator");
    return {};
}

//------------------------------------------------------------------------------
inline void setInvocationPolicyOption(Object& options, InvocationPolicy policy)
{
    CPPWAMP_LOGIC_CHECK(policy != InvocationPolicy::unknown,
                        "Cannot specify unknown invocation policy");

    if (policy == InvocationPolicy::single)
        options.erase("invoke");
    else
        options["invoke"] = toString(policy);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_INVOCATIONPOLICYOPTION_HPP
