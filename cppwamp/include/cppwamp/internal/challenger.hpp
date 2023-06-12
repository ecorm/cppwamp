/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CHALLENGER_HPP
#define CPPWAMP_INTERNAL_CHALLENGER_HPP

#include <memory>

namespace wamp
{

class Reason;
class SessionInfo;

namespace internal
{

//------------------------------------------------------------------------------
class Challenger
{
public:
    using WeakPtr = std::weak_ptr<Challenger>;

    virtual void safeChallenge() = 0;
    
    virtual void safeWelcome(SessionInfo&&) = 0;

    virtual void safeReject(Reason&&) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CHALLENGER_HPP
