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

#include "anyhandler.hpp"
#include "api.hpp"
#include "codec.hpp"
#include "listener.hpp"
#include "logging.hpp"
#include "peerdata.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "internal/challenger.hpp"

namespace wamp
{

namespace internal { class RouterServer; } // Forward declaration

//------------------------------------------------------------------------------
struct CPPWAMP_API AuthorizationInfo
{
public:
    using Ptr = std::shared_ptr<AuthorizationInfo>;

    explicit AuthorizationInfo(const Realm& realm, String role = "",
                               String method = "", String provider = "");

    SessionId sessionId() const;

    const String& realmUri() const;

    const String& id() const;

    const String& role() const;

    const String& method() const;

    const String& provider() const;

    Object welcomeDetails() const;

    void setSessionId(SessionId sid);

private:
    String realmUri_;
    String id_;
    String role_;
    String method_;
    String provider_;
    SessionId sessionId_ = 0;
};

//------------------------------------------------------------------------------
struct CPPWAMP_API AuthorizationRequest
{
    enum class Action
    {
        publish,
        subscribe,
        enroll,
        call
    };

    AuthorizationInfo::Ptr authInfo;
    Object options;
    String uri;
    Action action;
};

//------------------------------------------------------------------------------
class CPPWAMP_API RealmConfig
{
public:
    using AuthorizationHandler =
        AnyReusableHandler<bool (AuthorizationRequest)>;

    RealmConfig(String uri);

    RealmConfig& withAuthorizationHandler(AuthorizationHandler f);

    RealmConfig& withAuthorizationCacheEnabled(bool enabled = true);

    const String& uri() const;

    bool authorizationCacheEnabled() const;

private:
    AuthorizationHandler authorizationHandler_;
    String uri_;
    bool authorizationCacheEnabled_ = false;
};

namespace internal { class Challenger; } // Forward declaration

//------------------------------------------------------------------------------
/** Contains information on an authorization exchange with a router.  */
//------------------------------------------------------------------------------
class AuthExchange
{
public:
    using Ptr = std::shared_ptr<AuthExchange>;

    const Realm& realm() const;
    const Challenge& challenge() const;
    const Authentication& authentication() const;
    unsigned stage() const;
    const Variant& memento() const;

    void challenge(Challenge challenge, Variant memento = {});
    void challenge(ThreadSafe, Challenge challenge, Variant memento = {});
    void welcome(Object details = {});
    void welcome(ThreadSafe, Object details = {});
    void reject(Object details = {}, String reasonUri = {});
    void reject(ThreadSafe, Object details = {}, String reasonUri = {});

public:
    // Internal use only
    using ChallengerPtr = std::weak_ptr<internal::Challenger>;
    static Ptr create(internal::PassKey, Realm&& r, ChallengerPtr c);
    void setAuthentication(internal::PassKey, Authentication&& a);
    Challenge& accessChallenge(internal::PassKey);

private:
    AuthExchange(Realm&& r, ChallengerPtr c);

    Realm realm_;
    ChallengerPtr challenger_;
    Challenge challenge_;
    Authentication authentication_;
    Variant memento_; // Useful for keeping the authorizer stateless
    unsigned stage_ = 0;
};


//------------------------------------------------------------------------------
class CPPWAMP_API ServerConfig
{
public:
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
      listenerBuilder_(std::forward<S>(transportSettings))
{
    codecBuilders_ = {BufferCodecBuilder{format},
                      BufferCodecBuilder{extraFormats}...};
}

//------------------------------------------------------------------------------
class CPPWAMP_API RouterConfig
{
public:
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    RouterConfig& withLogHandler(LogHandler f);

    RouterConfig& withLogLevel(LogLevel l);

    RouterConfig& withSessionIdSeed(EphemeralId seed);

    const LogHandler& logHandler() const;

    LogLevel logLevel() const;

    EphemeralId sessionIdSeed() const;

private:
    LogHandler logHandler_;
    LogLevel logLevel_ = LogLevel::warning;
    EphemeralId sessionIdSeed_ = nullId();
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/routerconfig.ipp"
#endif

#endif // CPPWAMP_ROUTERCONFIG_HPP
