/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authenticator.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE const Petition& AuthExchange::hello() const {return hello_;}

CPPWAMP_INLINE const Challenge& AuthExchange::challenge() const
{
    return challenge_;
}

CPPWAMP_INLINE const Authentication& AuthExchange::authentication() const
{
    return authentication_;
}

CPPWAMP_INLINE unsigned AuthExchange::challengeCount() const
{
    return challengeCount_;
}

CPPWAMP_INLINE const any& AuthExchange::note() const & {return note_;}

CPPWAMP_INLINE any&& AuthExchange::note() && {return std::move(note_);}

CPPWAMP_INLINE void AuthExchange::challenge(Challenge challenge, any note)
{
    challenge_ = std::move(challenge);
    note_ = std::move(note);
    auto c = challenger_.lock();
    if (c)
    {
        ++challengeCount_;
        c->safeChallenge();
    }
}

CPPWAMP_INLINE void AuthExchange::welcome(AuthInfo info)
{
    auto c = challenger_.lock();
    if (c)
        c->safeWelcome(SessionInfo::create({}, std::move(info)));
}

CPPWAMP_INLINE void AuthExchange::reject(Reason r)
{
    auto c = challenger_.lock();
    if (c)
        c->safeReject(std::move(r));
}

CPPWAMP_INLINE AuthExchange::Ptr
AuthExchange::create(internal::PassKey, Petition&& p, ChallengerPtr c)
{
    return Ptr(new AuthExchange(std::move(p), std::move(c)));
}

CPPWAMP_INLINE void AuthExchange::setAuthentication(internal::PassKey,
                                                    Authentication&& a)
{
    authentication_ = std::move(a);
}

CPPWAMP_INLINE AuthExchange::AuthExchange(Petition&& p, ChallengerPtr c)
    : hello_(std::move(p)),
      challenger_(c)
{}

} // namespace wamp
