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

#include <memory>
#include "any.hpp"
#include "api.hpp"
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

public:
    // Internal use only
    void join(internal::PassKey, String realmUri, SessionId sessionId);
    Object join(internal::PassKey, String realmUri, SessionId sessionId,
                Object routerRoles);
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

    AuthInfo::Ptr authInfo;
    Object options;
    String uri;
    Action action;
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
