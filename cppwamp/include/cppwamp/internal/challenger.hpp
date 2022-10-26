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

    virtual void challenge(Challenge&& challenge, Variant&& memento) = 0;

    virtual void safeChallenge(Challenge&& challenge, Variant&& memento) = 0;

    virtual void welcome(Object details) = 0;

    virtual void safeWelcome(Object details) = 0;

    virtual void abortJoin(Object details) = 0;

    virtual void safeAbortJoin(Object details) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CHALLENGER_HPP
