/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
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
#include <string>
#include "anyhandler.hpp"
#include "api.hpp"
#include "authinfo.hpp"
#include "codec.hpp"
#include "listener.hpp"
#include "logging.hpp"
#include "wampdefs.hpp"

namespace wamp
{

namespace internal { class RouterServer; } // Forward declaration

//------------------------------------------------------------------------------
class CPPWAMP_API RealmConfig
{
public:
    using AuthorizedOp = AnyCompletionHandler<void (Authorization)>;

    using Authorizer =
        AnyReusableHandler<void (AuthorizationRequest, AuthorizedOp)>;

    RealmConfig(String uri);

    RealmConfig& withAuthorizer(Authorizer f);

    RealmConfig& withAuthorizationCacheEnabled(bool enabled = true);

    RealmConfig& withPublisherDisclosure(OriginatorDisclosure d);

    RealmConfig& withCallerDisclosure(OriginatorDisclosure d);

    const String& uri() const;

    const Authorizer& authorizer() const;

    bool authorizationCacheEnabled() const;

private:
    struct DefaultAuthorizer;

    Authorizer authorizer_;
    String uri_;
    OriginatorDisclosure publisherDisclosure_;
    OriginatorDisclosure callerDisclosure_;
    bool authorizationCacheEnabled_ = false;
};

namespace internal { class Challenger; } // Forward declaration

//------------------------------------------------------------------------------
class CPPWAMP_API AnonymousAuthenticator
{
public:
    AnonymousAuthenticator()
        : authId_("anonymous"),
          authRole_("public")
    {}

    AnonymousAuthenticator(String authId, String authRole)
        : authId_(std::move(authId)),
          authRole_(std::move(authRole))
    {}

    void operator()(AuthExchange::Ptr ex)
    {
        ex->welcome({authId_, authRole_, "anonymous", "static"});
    }

private:
    String authId_;
    String authRole_;
};

//------------------------------------------------------------------------------
class CPPWAMP_API ServerConfig
{
public:
    // TODO: IP filter
    // TODO: Authentication cooldown
    using Ptr = std::shared_ptr<ServerConfig>;
    using AuthExchangeHandler = AnyReusableHandler<void (AuthExchange::Ptr)>;

    template <typename S, typename F, typename... Fs>
    explicit ServerConfig(String name, S&& transportSettings, F format,
                          Fs... extraFormats);

    template <typename... TFormats>
    ServerConfig& withFormats(TFormats... formats);

    ServerConfig& withAuthenticator(AuthExchangeHandler f);

    const String& name() const;

    const AuthExchangeHandler& authenticator() const;

private:
    Listening::Ptr makeListener(IoStrand s) const;

    AnyBufferCodec makeCodec(int codecId) const;

    String name_;
    ListenerBuilder listenerBuilder_;
    std::vector<BufferCodecBuilder> codecBuilders_;
    AuthExchangeHandler authenticator_;

    friend class internal::RouterServer;
};

template <typename S, typename F, typename... Fs>
ServerConfig::ServerConfig(String name, S&& transportSettings, F format,
                           Fs... extraFormats)
    : name_(std::move(name)),
      listenerBuilder_(std::forward<S>(transportSettings)),
      authenticator_(AnonymousAuthenticator{})
{
    codecBuilders_ = {BufferCodecBuilder{format},
                      BufferCodecBuilder{extraFormats}...};
}

// TODO: User-provided PRNG

//------------------------------------------------------------------------------
class CPPWAMP_API RouterConfig
{
public:
    using LogHandler = AnyReusableHandler<void (LogEntry)>;
    using AccessLogHandler = AnyReusableHandler<void (AccessLogEntry)>;

    RouterConfig& withLogHandler(LogHandler f);

    RouterConfig& withLogLevel(LogLevel l);

    RouterConfig& withAccessLogHandler(AccessLogHandler f);

    RouterConfig& withAccessLogFilter(AccessLogFilter f);

    RouterConfig& withSessionIdSeed(EphemeralId seed);

    const LogHandler& logHandler() const;

    LogLevel logLevel() const;

    const AccessLogHandler& accessLogHandler() const;

    const AccessLogFilter& accessLogFilter() const;

    EphemeralId sessionIdSeed() const;

private:
    LogHandler logHandler_;
    AccessLogHandler accessLogHandler_;
    AccessLogFilter accessLogFilter_;
    LogLevel logLevel_ = LogLevel::warning;
    EphemeralId sessionIdSeed_ = nullId();
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/routerconfig.ipp"
#endif

#endif // CPPWAMP_ROUTERCONFIG_HPP
