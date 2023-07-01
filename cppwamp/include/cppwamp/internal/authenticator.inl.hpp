/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authenticator.hpp"
#include <utility>
#include "../api.hpp"
#include "sessioninfoimpl.hpp"

namespace wamp
{

//******************************************************************************
// AuthExchange
//******************************************************************************

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
        c->safeWelcome(internal::SessionInfoImpl::create(std::move(info)));
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

CPPWAMP_INLINE Petition& AuthExchange::hello(internal::PassKey) {return hello_;}

CPPWAMP_INLINE AuthExchange::AuthExchange(Petition&& p, ChallengerPtr c)
    : hello_(std::move(p)),
      challenger_(std::move(c))
{}


//******************************************************************************
// Authenticator
//******************************************************************************

/** @details
    This method makes it so that the `onAuthenticate` handler will be posted
    via the given executor. If no executor is set, the `onAuthenticate` handler
    is executed directly from the server's execution strand. */
CPPWAMP_INLINE void Authenticator::bindExecutor(AnyCompletionExecutor e)
{
    executor_ = std::move(e);
}

CPPWAMP_INLINE void Authenticator::authenticate(AuthExchange::Ptr exchange,
                                                AnyIoExecutor& ioExec)
{
    if (executor_ == nullptr)
    {
        onAuthenticate(std::move(exchange));
    }
    else
    {
        auto self = shared_from_this();
        boost::asio::post(
            ioExec,
            boost::asio::bind_executor(
                executor_,
                [this, self, exchange]() {onAuthenticate(exchange);} ));
    }
}

} // namespace wamp
