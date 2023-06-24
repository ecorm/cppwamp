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

CPPWAMP_INLINE RealmConfig::RealmConfig(Uri uri)
    : uri_(std::move(uri))
{}

CPPWAMP_INLINE RealmConfig&
RealmConfig::withAuthorizer(Authorizer::Ptr a)
{
    authorizer_ = std::move(a);
    return *this;
}

//CPPWAMP_INLINE RealmConfig&
//RealmConfig::withAuthorizationCacheEnabled(bool enabled)
//{
//    authorizationCacheEnabled_ = enabled;
//    return *this;
//}

/** @note DisclosureRule::preset is treated as DisclosureRule::originator. */
CPPWAMP_INLINE RealmConfig&
RealmConfig::withPublisherDisclosure(DisclosureRule d)
{
    publisherDisclosure_ =
        (d == DisclosureRule::preset) ? DisclosureRule::originator : d;
    return *this;
}

/** @note DisclosureRule::preset is treated as DisclosureRule::originator. */
CPPWAMP_INLINE RealmConfig&
RealmConfig::withCallerDisclosure(DisclosureRule d)
{
    callerDisclosure_ =
        (d == DisclosureRule::preset) ? DisclosureRule::originator : d;
    return *this;
}

CPPWAMP_INLINE RealmConfig& RealmConfig::withMetaApiEnabled(bool enabled)
{
    metaApiEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE const Uri& RealmConfig::uri() const {return uri_;}

CPPWAMP_INLINE Authorizer::Ptr RealmConfig::authorizer() const
{
    return authorizer_;
}

//CPPWAMP_INLINE bool RealmConfig::authorizationCacheEnabled() const
//{
//    return authorizationCacheEnabled_;
//}

CPPWAMP_INLINE DisclosureRule RealmConfig::publisherDisclosure() const
{
    return publisherDisclosure_;
}

CPPWAMP_INLINE DisclosureRule RealmConfig::callerDisclosure() const
{
    return publisherDisclosure_;
}

CPPWAMP_INLINE bool RealmConfig::metaApiEnabled() const
{
    return metaApiEnabled_;
}


//******************************************************************************
// ServerConfig
//******************************************************************************

CPPWAMP_INLINE ServerConfig&
ServerConfig::withAuthenticator(Authenticator::Ptr a)
{
    authenticator_ = std::move(a);
    return *this;
}

CPPWAMP_INLINE const String& ServerConfig::name() const {return name_;}

CPPWAMP_INLINE Authenticator::Ptr ServerConfig::authenticator() const
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

CPPWAMP_INLINE RouterConfig& RouterConfig::withLogLevel(LogLevel level)
{
    logLevel_ = level;
    return *this;
}

CPPWAMP_INLINE RouterConfig&
RouterConfig::withAccessLogHandler(AccessLogHandler f)
{
    accessLogHandler_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE RouterConfig& RouterConfig::withUriValidator(UriValidator::Ptr v)
{
    uriValidator_ = std::move(v);
    return *this;
}

CPPWAMP_INLINE RouterConfig&
RouterConfig::withSessionRNG(RandomNumberGenerator64 f)
{
    sessionRng_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE RouterConfig&
RouterConfig::withPublicationRNG(RandomNumberGenerator64 f)
{
    publicationRng_ = std::move(f);
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

CPPWAMP_INLINE UriValidator::Ptr RouterConfig::uriValidator() const
{
    return uriValidator_;
}

CPPWAMP_INLINE const RandomNumberGenerator64& RouterConfig::sessionRNG() const
{
    return sessionRng_;
}

CPPWAMP_INLINE const RandomNumberGenerator64&
RouterConfig::publicationRNG() const
{
    return publicationRng_;
}

CPPWAMP_INLINE void RouterConfig::initialize(internal::PassKey)
{
    if (!uriValidator_)
        uriValidator_ = RelaxedUriValidator::create();
}

} // namespace wamp
