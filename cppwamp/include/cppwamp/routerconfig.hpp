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

namespace wamp
{

namespace internal { class RouterServer; } // Forward declaration

//------------------------------------------------------------------------------
class CPPWAMP_API RealmConfig
{
public:
    RealmConfig(Uri uri);

    RealmConfig& withAuthorizer(Authorizer::Ptr a);

    // TODO: Authorization cache
    // RealmConfig& withAuthorizationCacheEnabled(bool enabled = true);

    // TODO: Progressive calls quota
    // TODO: Pending call quota

    RealmConfig& withPublisherDisclosure(DisclosureRule d);

    RealmConfig& withCallerDisclosure(DisclosureRule d);

    RealmConfig& withMetaApiEnabled(bool enabled = true);

    const Uri& uri() const;

    Authorizer::Ptr authorizer() const;

    // bool authorizationCacheEnabled() const;

    DisclosureRule publisherDisclosure() const;

    DisclosureRule callerDisclosure() const;

    bool metaApiEnabled() const;

private:
    Uri uri_;
    Authorizer::Ptr authorizer_;
    DisclosureRule publisherDisclosure_ = DisclosureRule::originator;
    DisclosureRule callerDisclosure_ = DisclosureRule::originator;
    bool metaApiEnabled_ = false;
    // bool authorizationCacheEnabled_ = false;
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
    explicit ServerConfig(String name, S&& transportSettings, F format,
                          Fs... extraFormats);

    template <typename... TFormats>
    ServerConfig& withFormats(TFormats... formats);

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
ServerConfig::ServerConfig(String name, S&& transportSettings, F format,
                           Fs... extraFormats)
    : name_(std::move(name)),
      listenerBuilder_(std::forward<S>(transportSettings))
{
    codecBuilders_ = {BufferCodecBuilder{format},
                      BufferCodecBuilder{extraFormats}...};
}

//------------------------------------------------------------------------------
using RandomNumberGenerator64 = std::function<uint64_t ()>;

//------------------------------------------------------------------------------
class CPPWAMP_API RouterConfig
{
public:
    /// Type-erases a LogEntry handler and its associated executor.
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    /// Type-erases an AccessLogEntry handler and its associated executor.
    using AccessLogHandler = AnyReusableHandler<void (AccessLogEntry)>;

    RouterConfig& withLogHandler(LogHandler f);

    RouterConfig& withLogLevel(LogLevel l);

    RouterConfig& withAccessLogHandler(AccessLogHandler f);

    RouterConfig& withUriValidator(UriValidator::Ptr v);

    RouterConfig& withSessionRNG(RandomNumberGenerator64 f);

    // This RNG needs to be distinct from session RNG because they
    // can be invoked from different threads.
    RouterConfig& withPublicationRNG(RandomNumberGenerator64 f);

    const LogHandler& logHandler() const;

    LogLevel logLevel() const;

    const AccessLogHandler& accessLogHandler() const;

    UriValidator::Ptr uriValidator() const;

    const RandomNumberGenerator64& sessionRNG() const;

    const RandomNumberGenerator64& publicationRNG() const;

private:
    LogHandler logHandler_;
    AccessLogHandler accessLogHandler_;
    UriValidator::Ptr uriValidator_;
    RandomNumberGenerator64 sessionRng_;
    RandomNumberGenerator64 publicationRng_;
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
