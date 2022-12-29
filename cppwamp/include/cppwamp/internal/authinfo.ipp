/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authinfo.hpp"
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// AuthorizationInfo
//******************************************************************************

CPPWAMP_INLINE AuthInfo::AuthInfo() {}

CPPWAMP_INLINE AuthInfo::AuthInfo(String id, String role, String method,
                                  String provider)
    : id_(std::move(id)),
      role_(std::move(role)),
      method_(std::move(method)),
      provider_(std::move(provider))
{}

CPPWAMP_INLINE AuthInfo& AuthInfo::withExtra(Object extra)
{
    extra_ = std::move(extra);
    return *this;
}

CPPWAMP_INLINE AuthInfo& AuthInfo::withNote(any note)
{
    note_ = std::move(note);
    return *this;
}

CPPWAMP_INLINE SessionId AuthInfo::sessionId() const {return sessionId_;}

CPPWAMP_INLINE const String& AuthInfo::realmUri() const {return realmUri_;}

CPPWAMP_INLINE const String& AuthInfo::id() const {return id_;}

CPPWAMP_INLINE const String& AuthInfo::role() const {return role_;}

CPPWAMP_INLINE const String& AuthInfo::method() const {return method_;}

CPPWAMP_INLINE const String& AuthInfo::provider() const {return provider_;}

CPPWAMP_INLINE Object
AuthInfo::join(internal::PassKey, String realmUri, SessionId sessionId,
               Object routerRoles)
{
    sessionId_ = sessionId;
    realmUri_ = std::move(realmUri);

    Object details;
    if (!id_.empty())
        details.emplace("authid", id_);
    if (!role_.empty())
        details.emplace("authrole", role_);
    if (!method_.empty())
        details.emplace("authmethod", method_);
    if (!provider_.empty())
        details.emplace("authprovider", provider_);
    if (!extra_.empty())
        details.emplace("authextra", std::move(extra_));
    details.emplace("roles", std::move(routerRoles));
    extra_.clear();
    return details;
}


//******************************************************************************
// AuthExchange
//******************************************************************************

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

CPPWAMP_INLINE const any& AuthExchange::memento() const & {return memento_;}

CPPWAMP_INLINE any&& AuthExchange::memento() && {return std::move(memento_);}

CPPWAMP_INLINE void AuthExchange::challenge(Challenge challenge, any memento)
{
    challenge_ = std::move(challenge);
    memento_ = std::move(memento);
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
    memento_ = std::move(memento);
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
