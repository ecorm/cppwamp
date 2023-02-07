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
// RealmConfig
//******************************************************************************

struct RealmConfig::DefaultAuthorizer
{
    void operator()(AuthorizationRequest, AuthorizedOp op) const
    {
        op(Authorization{true});
    }
};

CPPWAMP_INLINE RealmConfig::RealmConfig(String uri)
    : authorizer_(DefaultAuthorizer{}),
      uri_(std::move(uri))
{}

CPPWAMP_INLINE RealmConfig&
RealmConfig::withAuthorizer(Authorizer f)
{
    authorizer_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE RealmConfig&
RealmConfig::withAuthorizationCacheEnabled(bool enabled)
{
    authorizationCacheEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE RealmConfig&
RealmConfig::withPublisherDisclosure(OriginatorDisclosure d)
{
    publisherDisclosure_ = d;
    return *this;
}

CPPWAMP_INLINE RealmConfig&
RealmConfig::withCallerDisclosure(OriginatorDisclosure d)
{
    callerDisclosure_ = d;
    return *this;
}

CPPWAMP_INLINE const String& RealmConfig::uri() const {return uri_;}

CPPWAMP_INLINE const RealmConfig::Authorizer& RealmConfig::authorizer() const
{
    return authorizer_;
}

CPPWAMP_INLINE bool RealmConfig::authorizationCacheEnabled() const
{
    return authorizationCacheEnabled_;
}


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

CPPWAMP_INLINE RouterConfig&
RouterConfig::withAccessLogHandler(AccessLogHandler f)
{
    accessLogHandler_ = std::move(f);
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

CPPWAMP_INLINE const RouterConfig::AccessLogHandler&
RouterConfig::accessLogHandler() const
{
    return accessLogHandler_;
}

CPPWAMP_INLINE EphemeralId RouterConfig::sessionIdSeed() const
{
    return sessionIdSeed_;
}

} // namespace wamp
