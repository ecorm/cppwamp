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

namespace wamp
{

namespace internal { class RouterServer; } // Forward declaration

//------------------------------------------------------------------------------
struct CPPWAMP_API AuthorizationInfo
{
public:
    using Ptr = std::shared_ptr<AuthorizationInfo>;

    explicit AuthorizationInfo(const Realm& realm, String role = "",
                               String method = "", String provider = "")
        : realmUri_(realm.uri()),
          id_(realm.authId().value_or("")),
          role_(std::move(role)),
          method_(std::move(method)),
          provider_(std::move(provider))
    {}

    SessionId sessionId() const {return sessionId_;}

    const String& realmUri() const {return realmUri_;}

    const String& id() const {return id_;}

    const String& role() const {return role_;}

    const String& method() const {return method_;}

    const String& provider() const {return provider_;}

    Object welcomeDetails() const
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

    void setSessionId(SessionId sid) {sessionId_ = sid;}

private:
    String realmUri_;
    String id_;
    String role_;
    String method_;
    String provider_;
    SessionId sessionId_ = 0;
};

//------------------------------------------------------------------------------
struct AuthorizationRequest
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

    RealmConfig(String uri) : uri_(std::move(uri)) {}

    RealmConfig& withAuthorizationHandler(AuthorizationHandler f)
    {
        authorizationHandler_ = std::move(f);
        return *this;
    }

    RealmConfig& withAuthorizationCacheEnabled(bool enabled = true)
    {
        authorizationCacheEnabled_ = enabled;
        return *this;
    }

    const String& uri() const {return uri_;}

    bool authorizationCacheEnabled() const {return authorizationCacheEnabled_;}

private:
    AuthorizationHandler authorizationHandler_;
    String uri_;
    bool authorizationCacheEnabled_ = false;
};

//------------------------------------------------------------------------------
class CPPWAMP_API ServerConfig
{
public:
    using AuthExchangeHandler = AnyReusableHandler<void (AuthExchange::Ptr)>;

    template <typename S>
    explicit ServerConfig(String name, S&& transportSettings)
        : name_(std::move(name)),
        listenerBuilder_(std::forward<S>(transportSettings))
    {}

    template <typename... TFormats>
    ServerConfig& withFormats(TFormats... formats)
    {
        codecBuilders_ = {BufferCodecBuilder{formats}...};
        return *this;
    }

    ServerConfig& withAuthenticator(AuthExchangeHandler f)
    {
        authenticator_ = std::move(f);
        return *this;
    }

    const String& name() const {return name_;}

    const AuthExchangeHandler& authenticator() const {return authenticator_;}

private:
    Listening::Ptr makeListener(IoStrand s) const
    {
        std::set<int> codecIds;
        for (const auto& c: codecBuilders_)
            codecIds.emplace(c.id());
        return listenerBuilder_(std::move(s), std::move(codecIds));
    }

    AnyBufferCodec makeCodec(int codecId) const
    {
        for (const auto& c: codecBuilders_)
            if (c.id() == codecId)
                return c();
        assert(false);
        return {};
    }

    String name_;
    ListenerBuilder listenerBuilder_;
    std::vector<BufferCodecBuilder> codecBuilders_;
    AuthExchangeHandler authenticator_;

    friend class internal::RouterServer;
};

//------------------------------------------------------------------------------
class CPPWAMP_API RouterConfig
{
public:
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    RouterConfig& withLogHandler(LogHandler f)
    {
        logHandler_ = std::move(f);
        return *this;
    }

    RouterConfig& withLogLevel(LogLevel l)
    {
        logLevel_ = l;
        return *this;
    }

    const LogHandler& logHandler() const {return logHandler_;}

    LogLevel logLevel() const {return logLevel_;}

    // TODO: SessionID seed

private:
    LogHandler logHandler_;
    LogLevel logLevel_ = LogLevel::warning;
};

} // namespace wamp

#endif // CPPWAMP_ROUTERCONFIG_HPP
