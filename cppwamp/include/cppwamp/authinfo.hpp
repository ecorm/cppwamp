/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_AUTHINFO_HPP
#define CPPWAMP_AUTHINFO_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the API used by a _router_ peer in WAMP applications. */
//------------------------------------------------------------------------------

#include <cassert>
#include <memory>
#include "any.hpp"
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "peerdata.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "internal/challenger.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
class CPPWAMP_API AuthInfo
{
public:
    using Ptr = std::shared_ptr<AuthInfo>;

    AuthInfo();

    AuthInfo(String id, String role, String method, String provider);

    AuthInfo& withExtra(Object extra);

    AuthInfo& withNote(any note);

    SessionId sessionId() const;

    const String& realmUri() const;

    const String& id() const;

    const String& role() const;

    const String& method() const;

    const String& provider() const;

    bool isLocal() const;

    const any& note() const;

private:
    String realmUri_;
    String id_;
    String role_;
    String method_;
    String provider_;
    Object extra_;
    any note_;
    SessionId sessionId_ = nullId();
    bool isLocal_ = false;

public:
    // Internal use only
    void join(internal::PassKey, String realmUri, SessionId sessionId,
              bool isLocal);
    Object join(internal::PassKey, String realmUri, SessionId sessionId,
                Object routerRoles);
};

//------------------------------------------------------------------------------
enum class OriginatorDisclosure
{
    preset,     ///< Disclose originator as per the realm configuration preset.
    originator, ///< Disclose originator as per its `disclose_me` option.
    off,        ///< Don't disclose originator.
    on          ///< Disclose originator.
};

//------------------------------------------------------------------------------
struct CPPWAMP_API Authorization
{
    Authorization(bool allowed = true) : allowed_(allowed) {}

    Authorization(std::error_code ec) : error_(ec) {}

    Authorization& withTrustLevel(TrustLevel tl)
    {
        trustLevel_ = tl;
        return *this;
    }

    Authorization& withDisclosure(OriginatorDisclosure d)
    {
        disclosure_ = d;
        return *this;
    }

    std::error_code error() const {return error_;}

    bool allowed() const {return allowed_;}

    bool hasTrustLevel() const {return trustLevel_ >= 0;}

    TrustLevel trustLevel() const {return trustLevel_;}

    OriginatorDisclosure disclosure() const {return disclosure_;}

private:
    any data_;
    TrustLevel trustLevel_ = -1;
    OriginatorDisclosure disclosure_ = OriginatorDisclosure::preset;
    std::error_code error_;
    bool allowed_ = false;

public:
    // Internal use only
    void setData(internal::PassKey, any&& data) {data_ = std::move(data);}

    template <typename T>
    T& dataAs(internal::PassKey)
    {
        auto ptr = any_cast<T>(&data_);
        assert(ptr != nullptr);
        return *ptr;
    }
};

//------------------------------------------------------------------------------
enum class AuthorizationAction
{
    publish,
    subscribe,
    enroll,
    call
};

//------------------------------------------------------------------------------
class CPPWAMP_API AuthorizationRequest
{
public:
    using Action = AuthorizationAction;
    using Handler = AnyCompletionHandler<void (Authorization)>;

    Action action() const {return action_;}

    template <typename TPeerData>
    const TPeerData& dataAs()
    {
        auto ptr = any_cast<TPeerData>(&peerData_);
        CPPWAMP_LOGIC_CHECK(ptr != nullptr,
                            "Bad wamp::AuthorizationRequest::dataAs() type");
        return *ptr;
    }

    const AuthInfo& authInfo() const {return *authInfo_;}

    void authorize(Authorization a)
    {
        // TODO: Store weak pointer to RouterRealm instead and
        // call virtual handler function.
        a.setData({}, std::move(peerData_));
        postAny(strand_, std::move(handler_), std::move(a));
    }

    void allow() {authorize(true);}

    void deny() {authorize(false);}

    void fail(std::error_code ec) {authorize(Authorization{ec});}

private:
    any peerData_;
    IoStrand strand_;
    AnyCompletionHandler<void (Authorization)> handler_;
    AuthInfo::Ptr authInfo_;
    Action action_;

public:
    // Internal use only
    template <typename TPeerData>
    AuthorizationRequest(internal::PassKey, Action a, TPeerData&& d,
                         AuthInfo::Ptr i, IoStrand s, Handler h)
        : peerData_(std::forward<TPeerData>(d)),
          strand_(std::move(s)),
          handler_(std::move(h)),
          authInfo_(std::move(i)),
          action_(a)
    {}

    template <typename TPeerData>
    TPeerData& dataAs(internal::PassKey)
    {
        auto ptr = any_cast<TPeerData>(&peerData_);
        assert(ptr != nullptr);
        return *ptr;
    }
};

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
    unsigned challengeCount() const;
    const any& memento() const &;
    any&& memento() &&;

    void challenge(Challenge challenge, any memento = {});

    void challenge(ThreadSafe, Challenge challenge, any memento = {});

    void welcome(AuthInfo info);

    void welcome(ThreadSafe, AuthInfo info);

    void reject(Abort a = {SessionErrc::cannotAuthenticate});

    void reject(ThreadSafe, Abort a = {SessionErrc::cannotAuthenticate});

public:
    // Internal use only
    using ChallengerPtr = std::weak_ptr<internal::Challenger>;
    static Ptr create(internal::PassKey, Realm&& r, ChallengerPtr c);
    void setAuthentication(internal::PassKey, Authentication&& a);

private:
    AuthExchange(Realm&& r, ChallengerPtr c);

    Realm realm_;
    ChallengerPtr challenger_;
    Challenge challenge_;
    Authentication authentication_;
    any memento_; // Keeps the authorizer stateless
    unsigned challengeCount_ = 0;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authinfo.ipp"
#endif

#endif // CPPWAMP_AUTHINFO_HPP
