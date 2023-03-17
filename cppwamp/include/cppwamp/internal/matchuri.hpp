/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_MATCH_URI_HPP
#define CPPWAMP_INTERNAL_MATCH_URI_HPP

#include <tuple>
#include "../pubsubinfo.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class MatchUri
{
public:
    using Policy = MatchPolicy;

    MatchUri() = default;

    explicit MatchUri(Uri uri, Policy p = Policy::unknown)
        : uri_(std::move(uri)),
          policy_(p)
    {}

    explicit MatchUri(const Topic& t) : MatchUri(t.uri(), t.matchPolicy()) {}

    explicit MatchUri(Topic&& t)
        : MatchUri(std::move(t).uri({}), t.matchPolicy())
    {}

    const Uri& uri() const {return uri_;}

    Policy policy() const {return policy_;}

    bool operator==(const MatchUri& rhs) const
    {
        return std::tie(policy_, uri_) == std::tie(rhs.policy_, rhs.uri_);
    }

    bool operator!=(const MatchUri& rhs) const
    {
        return std::tie(policy_, uri_) != std::tie(rhs.policy_, rhs.uri_);
    }

    bool operator<(const MatchUri& rhs) const
    {
        return std::tie(policy_, uri_) < std::tie(rhs.policy_, rhs.uri_);
    }

private:
    Uri uri_;
    Policy policy_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_MATCH_URI_HPP
