/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ROUTERCONFIG_HPP
#define CPPWAMP_ROUTERCONFIG_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the API used by a _router_ peer in WAMP applications. */
//------------------------------------------------------------------------------

#include <memory>
#include <utility>
#include <vector>
#include "anyhandler.hpp"
#include "api.hpp"
#include "authenticator.hpp"
#include "authorizer.hpp"
#include "codec.hpp"
#include "listener.hpp"
#include "logging.hpp"
#include "uri.hpp"
#include "internal/passkey.hpp"

// TODO: Rename config -> options

namespace wamp
{

namespace internal { class RouterServer; } // Forward declaration

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
class CPPWAMP_API RealmConfig
{
public:
    RealmConfig(Uri uri); // NOLINT(google-explicit-constructor)

    RealmConfig& withAuthorizer(Authorizer::Ptr a);

    RealmConfig& withCallTimeoutForwardingRule(CallTimeoutForwardingRule rule);

    RealmConfig& withCallerDisclosure(DisclosureRule d);

    RealmConfig& withPublisherDisclosure(DisclosureRule d);

    RealmConfig& withMetaApiEnabled(bool enabled = true);

    RealmConfig& withMetaProcedureRegistrationAllowed(bool allowed = true);

    RealmConfig& withMetaTopicPublicationAllowed(bool allowed = true);

    // TODO: Progressive calls quota
    // TODO: Pending call quota

    const Uri& uri() const;

    Authorizer::Ptr authorizer() const;

    DisclosureRule callerDisclosure() const;

    CallTimeoutForwardingRule callTimeoutForwardingRule() const;

    DisclosureRule publisherDisclosure() const;

    bool metaApiEnabled() const;

    bool metaProcedureRegistrationAllowed() const;

    bool metaTopicPublicationAllowed() const;

private:
    Uri uri_;
    Authorizer::Ptr authorizer_;
    DisclosureRule callerDisclosure_ = DisclosureRule::originator;
    DisclosureRule publisherDisclosure_ = DisclosureRule::originator;
    CallTimeoutForwardingRule callTimeoutForwardingRule_ =
        CallTimeoutForwardingRule::perRegistration;
    bool metaApiEnabled_ = false;
    bool metaProcedureRegistrationAllowed_ = false;
    bool metaTopicPublicationAllowed_ = false;
};

namespace internal { class Challenger; } // Forward declaration

//------------------------------------------------------------------------------
class CPPWAMP_API ServerConfig
{
public:
    // TODO: IP filter
    // TODO: Authentication cooldown
    using Ptr = std::shared_ptr<ServerConfig>;

    template <typename S, typename F, typename... Fs>
    explicit ServerConfig(String name, S&& transportSettings, F&& format,
                          Fs&&... extraFormats);

    ServerConfig& withAuthenticator(Authenticator::Ptr a);

    template <typename F, typename E>
    ServerConfig& withAuthenticator(F&& authenticator, E&& executor)
    {
        return withAuthenticator(std::forward<F>(authenticator),
                                 std::forward<E>(executor));
    }

    const String& name() const;

    Authenticator::Ptr authenticator() const;

private:
    Listening::Ptr makeListener(IoStrand s) const;

    AnyBufferCodec makeCodec(int codecId) const;

    String name_;
    ListenerBuilder listenerBuilder_;
    std::vector<BufferCodecBuilder> codecBuilders_;
    Authenticator::Ptr authenticator_;

    friend class internal::RouterServer;
};

template <typename S, typename F, typename... Fs>
ServerConfig::ServerConfig(String name, S&& transportSettings, F&& format,
                           Fs&&... extraFormats)
    : name_(std::move(name)),
      listenerBuilder_(std::forward<S>(transportSettings)),
      codecBuilders_({BufferCodecBuilder{std::forward<F>(format)},
                      BufferCodecBuilder{std::forward<Fs>(extraFormats)}...})
{}

//------------------------------------------------------------------------------
using RandomNumberGenerator64 = std::function<uint64_t ()>;

//------------------------------------------------------------------------------
using RandomNumberGeneratorFactory = std::function<RandomNumberGenerator64 ()>;

//------------------------------------------------------------------------------
class CPPWAMP_API RouterConfig
{
public:
    /// Type-erases a LogEntry handler and its associated executor.
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    /// Type-erases an AccessLogEntry handler and its associated executor.
    using AccessLogHandler = AnyReusableHandler<void (AccessLogEntry)>;

    RouterConfig& withLogHandler(LogHandler f);

    RouterConfig& withLogLevel(LogLevel level);

    RouterConfig& withAccessLogHandler(AccessLogHandler f);

    RouterConfig& withUriValidator(UriValidator::Ptr v);

    RouterConfig& withRngFactory(RandomNumberGeneratorFactory f);

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
#include "internal/routerconfig.inl.hpp"
#endif

#endif // CPPWAMP_ROUTERCONFIG_HPP
