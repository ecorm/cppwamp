/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CHALLENGER_HPP
#define CPPWAMP_INTERNAL_CHALLENGER_HPP

#include <memory>
#include "../variant.hpp"

namespace wamp
{

class Challenge;

namespace internal
{

//------------------------------------------------------------------------------
class Challenger
{
public:
    using WeakPtr = std::weak_ptr<Challenger>;
    using ExchangeId = unsigned long long;

    virtual void challenge(ExchangeId id, Challenge&& challenge,
                           Variant&& memento) = 0;

    virtual void safeChallenge(ExchangeId id, Challenge&& challenge,
                               Variant&& memento) = 0;

    virtual void welcome(ExchangeId id, Object details) = 0;

    virtual void safeWelcome(ExchangeId id, Object details) = 0;

    virtual void abortJoin(ExchangeId id, Object details) = 0;

    virtual void safeAbortJoin(ExchangeId id, Object details) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CHALLENGER_HPP
