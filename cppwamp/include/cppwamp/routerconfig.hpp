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
    void welcome(Object details);
    void welcome(ThreadSafe, Object details);
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

    // With seed == nullid(), the random generator state is initialized
    // with system entropy.
    RouterConfig& withSessionIdSeed(EphemeralId seed)
    {
        sessionIdSeed_ = seed;
        return *this;
    }

    const LogHandler& logHandler() const {return logHandler_;}

    LogLevel logLevel() const {return logLevel_;}

    EphemeralId sessionIdSeed() const {return sessionIdSeed_;}

private:
    LogHandler logHandler_;
    LogLevel logLevel_ = LogLevel::warning;
    EphemeralId sessionIdSeed_ = nullId();
};

//******************************************************************************
// AuthExchange
//******************************************************************************

CPPWAMP_INLINE const Realm& AuthExchange::realm() const {return realm_;}

CPPWAMP_INLINE const Challenge& AuthExchange::challenge() const
{
    return challenge_;
}

CPPWAMP_INLINE const Authentication& AuthExchange::authentication() const
{
    return authentication_;
}

CPPWAMP_INLINE unsigned AuthExchange::stage() const {return stage_;}

CPPWAMP_INLINE const Variant& AuthExchange::memento() const {return memento_;}

CPPWAMP_INLINE void AuthExchange::challenge(Challenge challenge,
                                            Variant memento)
{
    challenge_ = std::move(challenge);
    memento_ = std::move(memento);
    auto c = challenger_.lock();
    if (c)
    {
        ++stage_;
        c->challenge();
    }
}

CPPWAMP_INLINE void AuthExchange::challenge(ThreadSafe, Challenge challenge,
                                            Variant memento)
{
    challenge_ = std::move(challenge);
    memento_ = std::move(memento);
    auto c = challenger_.lock();
    if (c)
    {
        ++stage_;
        c->safeChallenge();
    }
}

CPPWAMP_INLINE void AuthExchange::welcome(Object details)
{
    auto c = challenger_.lock();
    if (c)
        c->welcome(std::move(details));
}

CPPWAMP_INLINE void AuthExchange::welcome(ThreadSafe, Object details)
{
    auto c = challenger_.lock();
    if (c)
        c->safeWelcome(std::move(details));
}

CPPWAMP_INLINE void AuthExchange::reject(Object details, String reasonUri)
{
    auto c = challenger_.lock();
    if (c)
    {
        if (reasonUri.empty())
            reasonUri = "wamp.error.cannot_authenticate";
        c->reject(std::move(details), std::move(reasonUri));
    }
}

CPPWAMP_INLINE void AuthExchange::reject(ThreadSafe, Object details,
                                         String reasonUri)
{
    auto c = challenger_.lock();
    if (c)
    {
        if (reasonUri.empty())
            reasonUri = "wamp.error.cannot_authenticate";
        c->safeReject(std::move(details), std::move(reasonUri));
    }
}

CPPWAMP_INLINE AuthExchange::Ptr
AuthExchange::create(internal::PassKey, Realm&& r, ChallengerPtr c)
{
    return Ptr(new AuthExchange(std::move(r), std::move(c)));
}

CPPWAMP_INLINE void AuthExchange::setAuthentication(internal::PassKey,
                                                    Authentication&& a)
{
    authentication_ = std::move(a);
}

CPPWAMP_INLINE Challenge& AuthExchange::accessChallenge(internal::PassKey)
{
    return challenge_;
}

CPPWAMP_INLINE AuthExchange::AuthExchange(Realm&& r, ChallengerPtr c)
    : realm_(std::move(r)),
      challenger_(c)
{}



} // namespace wamp

#endif // CPPWAMP_ROUTERCONFIG_HPP
