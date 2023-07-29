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

#include <memory>
#include <utility>
#include <vector>
#include "anyhandler.hpp"
#include "api.hpp"
#include "authenticator.hpp"
#include "authorizer.hpp"
#include "codec.hpp"
#include "disclosure.hpp"
#include "listener.hpp"
#include "logging.hpp"
#include "uri.hpp"
#include "internal/passkey.hpp"

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

    // TODO: Progressive calls quota
    // TODO: Pending call quota

    const Uri& uri() const;

    Authorizer::Ptr authorizer() const;

    Disclosure callerDisclosure() const;

    CallTimeoutForwardingRule callTimeoutForwardingRule() const;

    Disclosure publisherDisclosure() const;

    bool metaApiEnabled() const;

    bool metaProcedureRegistrationAllowed() const;

    bool metaTopicPublicationAllowed() const;

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
};

namespace internal { class Challenger; } // Forward declaration

//------------------------------------------------------------------------------
class CPPWAMP_API ServerOptions
{
public:
    // TODO: IP filter
    // TODO: Authentication cooldown
    using Ptr = std::shared_ptr<ServerOptions>;

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
ServerOptions::ServerOptions(String name, S&& transportSettings, F&& format,
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
