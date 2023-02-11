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

CPPWAMP_INLINE const Realm& AuthExchange::realm() const {return realm_;}

CPPWAMP_INLINE const Challenge& AuthExchange::challenge() const
{
    return challenge_;
}

CPPWAMP_INLINE const Authentication& AuthExchange::authentication() const
{
    return authentication_;
}

CPPWAMP_INLINE unsigned AuthExchange::challengeCount() const {return challengeCount_;}

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
        c->challenge();
    }
}

CPPWAMP_INLINE void AuthExchange::challenge(ThreadSafe, Challenge challenge,
                                            any memento)
{
    challenge_ = std::move(challenge);
    note_ = std::move(memento);
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
        c->welcome(std::move(info));
}

CPPWAMP_INLINE void AuthExchange::welcome(ThreadSafe, AuthInfo info)
{
    auto c = challenger_.lock();
    if (c)
        c->safeWelcome(std::move(info));
}

CPPWAMP_INLINE void AuthExchange::reject(Abort a)
{
    auto c = challenger_.lock();
    if (c)
        c->reject(std::move(a));
}

CPPWAMP_INLINE void AuthExchange::reject(ThreadSafe, Abort a)
{
    auto c = challenger_.lock();
    if (c)
        c->safeReject(std::move(a));
}

CPPWAMP_INLINE AuthExchange::Ptr
AuthExchange::create(internal::PassKey, Realm&& r, ChallengerPtr c)
{
    return Ptr(new AuthExchange(std::move(r), std::move(c)));
}

CPPWAMP_INLINE void AuthExchange::setAuthentication(internal::PassKey,
                                                    Authentication&& a)
{
    authentication_ = std::move(a);
}

CPPWAMP_INLINE AuthExchange::AuthExchange(Realm&& r, ChallengerPtr c)
    : realm_(std::move(r)),
    challenger_(c)
{}

} // namespace wamp