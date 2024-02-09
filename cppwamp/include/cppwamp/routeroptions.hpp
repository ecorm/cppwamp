/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ROUTEROPTIONS_HPP
#define CPPWAMP_ROUTEROPTIONS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the API used by a _router_ peer in WAMP applications. */
//------------------------------------------------------------------------------

/* Security options wishlist:
    - Simultaneous connection limit per client IP
    - Pending call quota
    - Progressive calls quota
    - Publication quota
    - Subscription quota
    - Registration quota
    - IP allow/block lists
    - Authentication lockout/cooldown
    - Message rate limiting (https://github.com/wamp-proto/wamp-proto/issues/510)
*/

/* Other options wishlist
    - Telemetry at server and realm levels
*/

#include <memory>
#include <utility>
#include "anyhandler.hpp"
#include "api.hpp"
#include "authenticator.hpp"
#include "authorizer.hpp"
#include "codec.hpp"
#include "disclosure.hpp"
#include "listener.hpp"
#include "logging.hpp"
#include "routerlogger.hpp"
#include "timeout.hpp"
#include "uri.hpp"
#include "version.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Determines how call timeouts are forwarded to callees. */
//------------------------------------------------------------------------------
enum class CallTimeoutForwardingRule
{
    perRegistration, /**< Forward if and only if the `forward_timeouts` option
                          was set during procedure registration (default). */
    perFeature,      /**< Forward if and only if the callee announced support
                          for call timeouts under the `callee` role. */
    never            /**< Never forward call timeouts to callees and always
                          process them on the router. */
};

//------------------------------------------------------------------------------
class CPPWAMP_API RealmOptions
{
public:
    RealmOptions(Uri uri); // NOLINT(google-explicit-constructor)

    RealmOptions& withAuthorizer(Authorizer::Ptr a);

    RealmOptions& withCallTimeoutForwardingRule(CallTimeoutForwardingRule rule);

    RealmOptions& withCallerDisclosure(Disclosure d);

    RealmOptions& withPublisherDisclosure(Disclosure d);

    RealmOptions& withMetaApiEnabled(bool enabled = true);

    RealmOptions& withMetaProcedureRegistrationAllowed(bool allowed = true);

    RealmOptions& withMetaTopicPublicationAllowed(bool allowed = true);

    // https://github.com/wamp-proto/wamp-proto/issues/345
    RealmOptions& withPropagateXOptionsEnabled(bool enabled = true);

    const Uri& uri() const;

    Authorizer::Ptr authorizer() const;

    Disclosure callerDisclosure() const;

    CallTimeoutForwardingRule callTimeoutForwardingRule() const;

    Disclosure publisherDisclosure() const;

    bool metaApiEnabled() const;

    bool metaProcedureRegistrationAllowed() const;

    bool metaTopicPublicationAllowed() const;

    bool propagateXOptionsEnabled() const;

private:
    Uri uri_;
    Authorizer::Ptr authorizer_;
    Disclosure callerDisclosure_ = Disclosure::producer;
    Disclosure publisherDisclosure_ = Disclosure::producer;
    CallTimeoutForwardingRule callTimeoutForwardingRule_ =
        CallTimeoutForwardingRule::perRegistration;
    bool metaApiEnabled_ = false;
    bool metaProcedureRegistrationAllowed_ = false;
    bool metaTopicPublicationAllowed_ = false;
    bool propagateXOptions_ = false;
};

namespace internal { class Challenger; } // Forward declaration

//------------------------------------------------------------------------------
class CPPWAMP_API BinaryExponentialBackoff
{
public:
    constexpr BinaryExponentialBackoff();

    constexpr BinaryExponentialBackoff(Timeout fixedDelay)
        : min_(fixedDelay), max_(fixedDelay)
    {}

    constexpr BinaryExponentialBackoff(Timeout min, Timeout max)
        : min_(min), max_(max)
    {}

    constexpr Timeout min() const {return min_;}

    constexpr Timeout max() const {return max_;}

    constexpr bool isUnspecified() const {return min_ == unspecifiedTimeout;}

    BinaryExponentialBackoff& validate();

private:
    Timeout min_ = unspecifiedTimeout;
    Timeout max_ = unspecifiedTimeout;
};

//------------------------------------------------------------------------------
class CPPWAMP_API ServerOptions
{
public:
    using Ptr = std::shared_ptr<ServerOptions>;
    using Backoff = BinaryExponentialBackoff;

    template <typename S, typename F, typename... Fs>
    explicit ServerOptions(String name, S&& transportSettings, F&& format,
                           Fs&&... extraFormats);

    ServerOptions& withAuthenticator(Authenticator::Ptr a);

    template <typename F, typename E>
    ServerOptions& withAuthenticator(F&& authenticator, E&& executor)
    {
        return withAuthenticator(std::forward<F>(authenticator),
                                 std::forward<E>(executor));
    }

    ServerOptions& withAgent(String agent);

    ServerOptions& withSoftConnectionLimit(std::size_t limit);

    ServerOptions& withHardConnectionLimit(std::size_t limit);

    ServerOptions& withMonitoringInterval(Timeout timeout);

    ServerOptions& withHelloTimeout(Timeout timeout);

    ServerOptions& withChallengeTimeout(Timeout timeout);

    ServerOptions& withStaleTimeout(Timeout timeout);

    ServerOptions& withOverstayTimeout(Timeout timeout);

    ServerOptions& withAcceptBackoff(Backoff backoff);

    const String& name() const;

    Authenticator::Ptr authenticator() const;

    const String& agent() const;

    std::size_t softConnectionLimit() const;

    std::size_t hardConnectionLimit() const;

    Timeout monitoringInterval() const;

    Timeout helloTimeout() const;

    Timeout challengeTimeout() const;

    Timeout staleTimeout() const;

    /** Obtains the maximum allowable continuous connection time. */
    Timeout overstayTimeout() const;

    Backoff acceptBackoff() const;

    Backoff outageBackoff() const;

private:
    // TODO: Initialize magic numbers in source files
    String name_;
    String agent_;
    ListenerBuilder listenerBuilder_;
    BufferCodecFactory codecFactory_;
    Authenticator::Ptr authenticator_;
    std::size_t softConnectionLimit_ = 512; // Using Nginx's worker_connections
    std::size_t hardConnectionLimit_ = 768; // // Using soft limit + 50%
    Timeout monitoringInterval_ = std::chrono::seconds{1};
        // Apache httpd RequestReadTimeout has a 1-second granularity
    Timeout helloTimeout_ = std::chrono::seconds{30};
        // Using ejabberd's negotiation_timeout
    Timeout challengeTimeout_ = std::chrono::seconds{30};
        // Using ejabberd's negotiation_timeout
    Timeout staleTimeout_ = std::chrono::seconds{300};
        // Using ejabberd's websocket_timeout
    Timeout overstayTimeout_ = neverTimeout;
    Backoff acceptBackoff_ = {std::chrono::milliseconds{625},
                              std::chrono::seconds{10}};
        // Starts from approximately Nginx's accept_mutex_delay and ends with
        // an arbitrarily chosen max delay.

public: // Internal use only
    Listening::Ptr makeListener(internal::PassKey, AnyIoExecutor e,
                                IoStrand s, RouterLogger::Ptr l) const;

    AnyBufferCodec makeCodec(internal::PassKey, int id) const;
};

template <typename S, typename F, typename... Fs>
ServerOptions::ServerOptions(String name, S&& transportSettings, F&& format,
                             Fs&&... extraFormats)
    : name_(std::move(name)),
      agent_(Version::serverAgentString()),
      listenerBuilder_(std::forward<S>(transportSettings)),
      codecFactory_(format, extraFormats...)
{}

//------------------------------------------------------------------------------
using RandomNumberGenerator64 = std::function<uint64_t ()>;

//------------------------------------------------------------------------------
using RandomNumberGeneratorFactory = std::function<RandomNumberGenerator64 ()>;

//------------------------------------------------------------------------------
class CPPWAMP_API RouterOptions
{
public:
    /// Type-erases a LogEntry handler and its associated executor.
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    /// Type-erases an AccessLogEntry handler and its associated executor.
    using AccessLogHandler = AnyReusableHandler<void (AccessLogEntry)>;

    RouterOptions& withLogHandler(LogHandler f);

    RouterOptions& withLogLevel(LogLevel level);

    RouterOptions& withAccessLogHandler(AccessLogHandler f);

    RouterOptions& withUriValidator(UriValidator::Ptr v);

    RouterOptions& withRngFactory(RandomNumberGeneratorFactory f);

    const LogHandler& logHandler() const;

    LogLevel logLevel() const;

    const AccessLogHandler& accessLogHandler() const;

    UriValidator::Ptr uriValidator() const;

    const RandomNumberGeneratorFactory& rngFactory() const;

private:
    LogHandler logHandler_;
    AccessLogHandler accessLogHandler_;
    UriValidator::Ptr uriValidator_;
    RandomNumberGeneratorFactory rngFactory_;
    LogLevel logLevel_ = LogLevel::warning;

public:
    // Internal use only
    void initialize(internal::PassKey);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/routeroptions.inl.hpp"
#endif

#endif // CPPWAMP_ROUTEROPTIONS_HPP
