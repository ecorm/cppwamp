/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../routeroptions.hpp"
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// RealmOptions
//******************************************************************************

CPPWAMP_INLINE RealmOptions::RealmOptions(Uri uri)
    : uri_(std::move(uri))
{}

CPPWAMP_INLINE RealmOptions&
RealmOptions::withAuthorizer(Authorizer::Ptr a)
{
    authorizer_ = std::move(a);
    return *this;
}

CPPWAMP_INLINE RealmOptions&
RealmOptions::withCallTimeoutForwardingRule(CallTimeoutForwardingRule rule)
{
    callTimeoutForwardingRule_ = rule;
    return *this;
}

/** @note Disclosure::preset is treated as Disclosure::producer. */
CPPWAMP_INLINE RealmOptions& RealmOptions::withCallerDisclosure(Disclosure d)
{
    callerDisclosure_ = d;
    return *this;
}

/** @note Disclosure::preset is treated as Disclosure::producer. */
CPPWAMP_INLINE RealmOptions& RealmOptions::withPublisherDisclosure(Disclosure d)
{
    publisherDisclosure_ = d;
    return *this;
}

CPPWAMP_INLINE RealmOptions& RealmOptions::withMetaApiEnabled(bool enabled)
{
    metaApiEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE RealmOptions&
RealmOptions::withMetaProcedureRegistrationAllowed(bool allowed)
{
    metaProcedureRegistrationAllowed_ = allowed;
    return *this;
}

CPPWAMP_INLINE RealmOptions&
RealmOptions::withMetaTopicPublicationAllowed(bool allowed)
{
    metaTopicPublicationAllowed_ = allowed;
    return *this;
}


CPPWAMP_INLINE const Uri& RealmOptions::uri() const {return uri_;}

CPPWAMP_INLINE Authorizer::Ptr RealmOptions::authorizer() const
{
    return authorizer_;
}

CPPWAMP_INLINE Disclosure RealmOptions::callerDisclosure() const
{
    return callerDisclosure_;
}

CPPWAMP_INLINE Disclosure RealmOptions::publisherDisclosure() const
{
    return publisherDisclosure_;
}

CPPWAMP_INLINE CallTimeoutForwardingRule
RealmOptions::callTimeoutForwardingRule() const
{
    return callTimeoutForwardingRule_;
}

CPPWAMP_INLINE bool RealmOptions::metaApiEnabled() const
{
    return metaApiEnabled_;
}

CPPWAMP_INLINE bool RealmOptions::metaProcedureRegistrationAllowed() const
{
    return metaProcedureRegistrationAllowed_;
}

CPPWAMP_INLINE bool RealmOptions::metaTopicPublicationAllowed() const
{
    return metaTopicPublicationAllowed_;
}


//******************************************************************************
// BinaryExponentialBackoff
//******************************************************************************

/** @throws error::Logic if either or both of the backoff delays are
                negative, or if the max delay is shorter than the min delay. */
CPPWAMP_INLINE BinaryExponentialBackoff& BinaryExponentialBackoff::validate()
{
    CPPWAMP_LOGIC_CHECK(min_.count() >= 0,
                        "Delay must not be negative");
    CPPWAMP_LOGIC_CHECK(max_.count() >= 0,
                        "Delay must not be negative");
    CPPWAMP_LOGIC_CHECK(max_ >= min_,
                        "Max delay must not be shorter than min delay");
    return *this;
}


//******************************************************************************
// ServerOptions
//******************************************************************************

CPPWAMP_INLINE ServerOptions&
ServerOptions::withAuthenticator(Authenticator::Ptr a)
{
    authenticator_ = std::move(a);
    return *this;
}

CPPWAMP_INLINE ServerOptions& ServerOptions::withAgent(String agent)
{
    agent_ = std::move(agent);
    return *this;
}

CPPWAMP_INLINE ServerOptions&
ServerOptions::withSoftConnectionLimit(std::size_t limit)
{
    softConnectionLimit_ = limit;
    return *this;
}

CPPWAMP_INLINE ServerOptions&
ServerOptions::withHardConnectionLimit(std::size_t limit)
{
    hardConnectionLimit_ = limit;
    return *this;
}

/** @throws error::Logic if the given interval is negative. */
CPPWAMP_INLINE ServerOptions&
ServerOptions::withMonitoringInterval(Timeout interval)
{
    monitoringInterval_ = internal::checkTimeout(interval);
    return *this;
}

/** @throws error::Logic if the given timeout duration is negative. */
CPPWAMP_INLINE ServerOptions&
ServerOptions::withHelloTimeout(Timeout timeout)
{
    helloTimeout_ = internal::checkTimeout(timeout);
    return *this;
}

/** @throws error::Logic if the given timeout duration is negative. */
CPPWAMP_INLINE ServerOptions&
ServerOptions::withChallengeTimeout(Timeout timeout)
{
    challengeTimeout_ = internal::checkTimeout(timeout);
    return *this;
}

/** @throws error::Logic if either or both of the backoff delays are negative,
            or if the max delay is shorter than the min delay. */
CPPWAMP_INLINE ServerOptions&
ServerOptions::withAcceptBackoff(Backoff backoff)
{
    acceptBackoff_ = backoff.validate();
    return *this;
}

CPPWAMP_INLINE const String& ServerOptions::name() const {return name_;}

CPPWAMP_INLINE Authenticator::Ptr ServerOptions::authenticator() const
{
    return authenticator_;
}

CPPWAMP_INLINE const String& ServerOptions::agent() const {return agent_;}

CPPWAMP_INLINE std::size_t ServerOptions::softConnectionLimit() const
{
    return softConnectionLimit_;
}

CPPWAMP_INLINE std::size_t ServerOptions::hardConnectionLimit() const
{
    return hardConnectionLimit_;
}

CPPWAMP_INLINE Timeout ServerOptions::monitoringInterval() const
{
    return monitoringInterval_;
}

CPPWAMP_INLINE Timeout ServerOptions::helloTimeout() const
{
    return challengeTimeout_;
}

CPPWAMP_INLINE Timeout ServerOptions::challengeTimeout() const
{
    return challengeTimeout_;
}

CPPWAMP_INLINE ServerOptions::Backoff ServerOptions::acceptBackoff() const
{
    return acceptBackoff_;
}

CPPWAMP_INLINE Listening::Ptr ServerOptions::makeListener(
    internal::PassKey, AnyIoExecutor e, IoStrand s, RouterLogger::Ptr l) const
{
    return listenerBuilder_(std::move(e), std::move(s), codecFactory_.ids(),
                            std::move(l));
}

CPPWAMP_INLINE AnyBufferCodec ServerOptions::makeCodec(internal::PassKey,
                                                       int id) const
{
    return codecFactory_(id);
}


//******************************************************************************
// RouterOptions
//******************************************************************************

CPPWAMP_INLINE RouterOptions& RouterOptions::withLogHandler(LogHandler f)
{
    logHandler_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE RouterOptions& RouterOptions::withLogLevel(LogLevel level)
{
    logLevel_ = level;
    return *this;
}

CPPWAMP_INLINE RouterOptions&
RouterOptions::withAccessLogHandler(AccessLogHandler f)
{
    accessLogHandler_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE RouterOptions&
RouterOptions::withUriValidator(UriValidator::Ptr v)
{
    uriValidator_ = std::move(v);
    return *this;
}

CPPWAMP_INLINE RouterOptions&
RouterOptions::withRngFactory(RandomNumberGeneratorFactory f)
{
    rngFactory_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE const RouterOptions::LogHandler&
RouterOptions::logHandler() const
{
    return logHandler_;
}

CPPWAMP_INLINE LogLevel RouterOptions::logLevel() const {return logLevel_;}

CPPWAMP_INLINE const RouterOptions::AccessLogHandler&
RouterOptions::accessLogHandler() const
{
    return accessLogHandler_;
}

CPPWAMP_INLINE UriValidator::Ptr RouterOptions::uriValidator() const
{
    return uriValidator_;
}

CPPWAMP_INLINE const RandomNumberGeneratorFactory&
RouterOptions::rngFactory() const
{
    return rngFactory_;
}

CPPWAMP_INLINE void RouterOptions::initialize(internal::PassKey)
{
    if (!uriValidator_)
        uriValidator_ = RelaxedUriValidator::create();
}

} // namespace wamp
