/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../routerconfig.hpp"
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// AuthorizationInfo
//******************************************************************************

CPPWAMP_INLINE AuthorizationInfo::AuthorizationInfo(
    const Realm& realm, String role, String method, String provider)
    : realmUri_(realm.uri()),
      id_(realm.authId().value_or("")),
      role_(std::move(role)),
      method_(std::move(method)),
      provider_(std::move(provider))
{}

CPPWAMP_INLINE SessionId AuthorizationInfo::sessionId() const
{
    return sessionId_;
}

CPPWAMP_INLINE const String& AuthorizationInfo::realmUri() const
{
    return realmUri_;
}

CPPWAMP_INLINE const String& AuthorizationInfo::id() const {return id_;}

CPPWAMP_INLINE const String& AuthorizationInfo::role() const {return role_;}

CPPWAMP_INLINE const String& AuthorizationInfo::method() const
{
    return method_;
}

CPPWAMP_INLINE const String& AuthorizationInfo::provider() const
{
    return provider_;
}

CPPWAMP_INLINE Object AuthorizationInfo::welcomeDetails() const
{
    Object details;
    if (!id_.empty())
        details.emplace("authid", id_);
    if (!role_.empty())
        details.emplace("authrole", role_);
    if (!method_.empty())
        details.emplace("authmethod", method_);
    if (!provider_.empty())
        details.emplace("authprovider", provider_);
    return details;
}

CPPWAMP_INLINE void AuthorizationInfo::setSessionId(SessionId sid)
{
    sessionId_ = sid;
}


//******************************************************************************
// RealmConfig
//******************************************************************************

CPPWAMP_INLINE RealmConfig::RealmConfig(String uri) : uri_(std::move(uri)) {}

CPPWAMP_INLINE RealmConfig&
RealmConfig::withAuthorizationHandler(AuthorizationHandler f)
{
    authorizationHandler_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE RealmConfig&
RealmConfig::withAuthorizationCacheEnabled(bool enabled)
{
    authorizationCacheEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE const String& RealmConfig::uri() const {return uri_;}

CPPWAMP_INLINE bool RealmConfig::authorizationCacheEnabled() const
{
    return authorizationCacheEnabled_;
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

CPPWAMP_INLINE unsigned AuthExchange::stage() const {return stage_;}

CPPWAMP_INLINE const Variant& AuthExchange::memento() const {return memento_;}

CPPWAMP_INLINE void AuthExchange::challenge(Challenge challenge,
                                            Variant memento)
{
    challenge_ = std::move(challenge);
    memento_ = std::move(memento);
    auto c = challenger_.lock();
    if (c)
    {
        ++stage_;
        c->challenge();
    }
}

CPPWAMP_INLINE void AuthExchange::challenge(ThreadSafe, Challenge challenge,
                                            Variant memento)
{
    challenge_ = std::move(challenge);
    memento_ = std::move(memento);
    auto c = challenger_.lock();
    if (c)
    {
        ++stage_;
        c->safeChallenge();
    }
}

CPPWAMP_INLINE void AuthExchange::welcome(Object details)
{
    auto c = challenger_.lock();
    if (c)
        c->welcome(std::move(details));
}

CPPWAMP_INLINE void AuthExchange::welcome(ThreadSafe, Object details)
{
    auto c = challenger_.lock();
    if (c)
        c->safeWelcome(std::move(details));
}

CPPWAMP_INLINE void AuthExchange::reject(Object details, String reasonUri)
{
    auto c = challenger_.lock();
    if (c)
    {
        if (reasonUri.empty())
            reasonUri = "wamp.error.cannot_authenticate";
        c->reject(std::move(details), std::move(reasonUri));
    }
}

CPPWAMP_INLINE void AuthExchange::reject(ThreadSafe, Object details,
                                         String reasonUri)
{
    auto c = challenger_.lock();
    if (c)
    {
        if (reasonUri.empty())
            reasonUri = "wamp.error.cannot_authenticate";
        c->safeReject(std::move(details), std::move(reasonUri));
    }
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

CPPWAMP_INLINE Challenge& AuthExchange::accessChallenge(internal::PassKey)
{
    return challenge_;
}

CPPWAMP_INLINE AuthExchange::AuthExchange(Realm&& r, ChallengerPtr c)
    : realm_(std::move(r)),
    challenger_(c)
{}


//******************************************************************************
// ServerConfig
//******************************************************************************

CPPWAMP_INLINE ServerConfig&
ServerConfig::withAuthenticator(AuthExchangeHandler f)
{
    authenticator_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE const String& ServerConfig::name() const {return name_;}

CPPWAMP_INLINE const ServerConfig::AuthExchangeHandler&
ServerConfig::authenticator() const
{
    return authenticator_;
}

CPPWAMP_INLINE Listening::Ptr ServerConfig::makeListener(IoStrand s) const
{
    std::set<int> codecIds;
    for (const auto& c: codecBuilders_)
        codecIds.emplace(c.id());
    return listenerBuilder_(std::move(s), std::move(codecIds));
}

CPPWAMP_INLINE AnyBufferCodec ServerConfig::makeCodec(int codecId) const
{
    for (const auto& c: codecBuilders_)
        if (c.id() == codecId)
            return c();
    assert(false);
    return {};
}


//******************************************************************************
// RouterConfig
//******************************************************************************

CPPWAMP_INLINE RouterConfig& RouterConfig::withLogHandler(LogHandler f)
{
    logHandler_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE RouterConfig& RouterConfig::withLogLevel(LogLevel l)
{
    logLevel_ = l;
    return *this;
}

// With seed == nullid(), the random generator state is initialized
// with system entropy.
CPPWAMP_INLINE RouterConfig& RouterConfig::withSessionIdSeed(EphemeralId seed)
{
    sessionIdSeed_ = seed;
    return *this;
}

CPPWAMP_INLINE const RouterConfig::LogHandler& RouterConfig::logHandler() const
{
    return logHandler_;
}

CPPWAMP_INLINE LogLevel RouterConfig::logLevel() const {return logLevel_;}

CPPWAMP_INLINE EphemeralId RouterConfig::sessionIdSeed() const
{
    return sessionIdSeed_;
}

} // namespace wamp
