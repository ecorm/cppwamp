/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PEERDATA_HPP
#define CPPWAMP_PEERDATA_HPP

#include <future>
#include <memory>
#include <string>
#include <vector>
#include "accesslogging.hpp"
#include "api.hpp"
#include "errorcodes.hpp"
#include "erroror.hpp"
#include "features.hpp"
#include "options.hpp"
#include "payload.hpp"
#include "tagtypes.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"
#include "./internal/wampmessage.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Provides data structures for information exchanged via WAMP
           session management messages. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides the _reason_ URI and other options contained within
    `GOODBYE` and `ABORT` messages.*/
//------------------------------------------------------------------------------
class CPPWAMP_API Reason : public Options<Reason, internal::GoodbyeMessage>
{
public:
    /** Converting constructor taking an optional reason URI. */
    Reason(String uri = {});

    /** Converting constructor taking an error code, attempting to convert
        it to a URI. */
    Reason(std::error_code ec);

    /** Converting constructor taking a WampErrc, attempting to convert
        it to a reason URI. */
    Reason(WampErrc errc);

    /** Sets the `message` member of the details dictionary. */
    Reason& withHint(String message);

    /** Obtains the reason URI. */
    const String& uri() const;

    /** Obtains the `message` member of the details dictionary. */
    ErrorOr<String> hint() const;

    /** Attempts to convert the reason URI to a known error code. */
    WampErrc errorCode() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool isServer) const;

private:
    using Base = Options<Reason, internal::GoodbyeMessage>;

public:
    // Internal use only
    Reason(internal::PassKey, internal::GoodbyeMessage&& msg);
    Reason(internal::PassKey, internal::AbortMessage&& msg);
    void setUri(internal::PassKey, String uri);
    internal::AbortMessage& abortMessage(internal::PassKey);
};

//------------------------------------------------------------------------------
/** %Realm URI and other options contained within WAMP `HELLO` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Realm : public Options<Realm, internal::HelloMessage>
{
public:
    /** Converting constructor taking a realm URI. */
    Realm(String uri);

    /** Specifies the Reason object in which to store abort details returned
        by the router. */
    Realm& captureAbort(Reason& reason);

    /** Obtains the realm URI. */
    const String& uri() const;

    /** Obtains the agent string. */
    ErrorOr<String> agent() const;

    /** Obtains the roles dictionary. */
    ErrorOr<Object> roles() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Authentication
        See [Authentication Methods in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-authentication-methods)
        @{ */

    /** Sets the `HELLO.Details.authmethods` option. */
    Realm& withAuthMethods(std::vector<String> methods);

    /** Sets the `HELLO.Details.authid` option. */
    Realm& withAuthId(String authId);

    /** Obtains the `authmethods` array. */
    ErrorOr<Array> authMethods() const;

    /** Obtains the `authid` string, or an empty string if unavailable. */
    ErrorOr<String> authId() const;
    /// @}

private:
    using Base = Options<Realm, internal::HelloMessage>;

    Reason* abortReason_ = nullptr;

public:
    // Internal use only
    Realm(internal::PassKey, internal::HelloMessage&& msg);
    Reason* abortReason(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Session information contained within WAMP `WELCOME` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Welcome : public Options<Welcome, internal::WelcomeMessage>
{
public:
    /** A set of role strings. */
    using RoleSet = std::set<String>;

    /** A set of feature strings. */
    using FeatureSet = std::set<String>;

    /** A dictionary of feature sets to be supported by each role. */
    using FeatureMap = std::map<String, FeatureSet>;

    /** Default constructor. */
    Welcome();

    /** Obtains the WAMP session ID. */
    SessionId id() const;

    /** Obtains realm URI. */
    const String& realm() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Agent Identification
        See [Agent Identification in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-hello-2)
        @{ */

    /** Obtains the agent string of the WAMP router. */
    ErrorOr<String> agentString() const;
    /// @}

    /** @name Role and Feature Announcement
        See [Client: Role and Feature Announcement in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-hello-2)
        @{ */

    /** Obtains the `WELCOME.Details.roles` dictionary for the router. */
    ErrorOr<Object> roles() const;

    /** Obtains a parsed set of features supported by the router. */
    ErrorOr<RouterFeatures> features() const;

    /** Checks that the router supports the given set of roles. */
    bool supportsRoles(const RoleSet& roles) const;

    /** Checks that the router supports the given map of features. */
    bool supportsFeatures(const FeatureMap& features) const;
    /// @}

    /** @name Authentication
        See [Authentication Methods in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-authentication-methods)
        @{ */

    /** Obtains the authentication ID the client was actually
        authenticated as. */
    ErrorOr<String> authId() const;

    /** Obtains the role the client was authenticated for. */
    ErrorOr<String> authRole() const;

    /** Obtains the method that was used for authentication. */
    ErrorOr<String> authMethod() const;

    /** Obtains the authentication provider. */
    ErrorOr<String> authProvider() const;

    /** Obtains extra authentication details. */
    ErrorOr<Object> authExtra() const;
    /// @}

private:
    using Base = Options<Welcome, internal::WelcomeMessage>;

    String realm_;

public:
    // Internal use only
    Welcome(internal::PassKey, String&& realm, internal::WelcomeMessage&& msg);
};


//------------------------------------------------------------------------------
/** Provides the _Signature_ and _Extra_ dictionary contained within
    WAMP `AUTHENTICATE` messages.

    See [Authentication Methods in the WAMP specification]
    (https://wamp-proto.org/wamp_latest_ietf.html#name-authentication-methods) */
//------------------------------------------------------------------------------
class CPPWAMP_API Authentication : public Options<Authentication,
                                                  internal::AuthenticateMessage>
{
public:
    /** Constructs an authentication with an empty signature. */
    Authentication();

    /** Converting constructor taking the authentication signature. */
    Authentication(String signature);

    /** Obtains the authentication signature. */
    const String& signature() const;

    /** Sets the client-server nonce used with the WAMP-SCRAM
        authentication method. */
    Authentication& withNonce(std::string nonce);

    /** Sets the channel binding information used with the WAMP-SCRAM
        authentication method. */
    Authentication& withChannelBinding(std::string type, std::string data);

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

private:
    using Base = Options<Authentication, internal::AuthenticateMessage>;

public:
    // Internal use only
    Authentication(internal::PassKey, internal::AuthenticateMessage&& msg);
};



namespace internal { class Challengee; } // Forward declaration

//------------------------------------------------------------------------------
/** Provides the _AuthMethod_ and _Extra_ dictionary contained within
    WAMP `CHALLENGE` messages.

    See [Authentication Methods in the WAMP specification]
    (https://wamp-proto.org/wamp_latest_ietf.html#name-authentication-methods) */
//------------------------------------------------------------------------------
class CPPWAMP_API Challenge : public Options<Challenge,
                                             internal::ChallengeMessage>
{
public:
    /** Constructs a challenge. */
    Challenge(String authMethod = {});

    Challenge& withChallenge(String challenge);

    Challenge& withSalt(String salt);

    Challenge& withKeyLength(UInt keyLength);

    Challenge& withIterations(UInt iterations);

    Challenge& withKdf(String kdf);

    Challenge& withMemory(UInt memory);

    /** Determines if the Session object that dispatched this
        invocation still exists or has expired. */
    bool challengeeHasExpired() const;

    /** Obtains the authentication method string. */
    const String& method() const;

    /** Obtains the challenge string. */
    ErrorOr<String> challenge() const;

    /** Obtains the salt string. */
    ErrorOr<String> salt() const;

    /** Obtains the key length. */
    ErrorOr<UInt> keyLength() const;

    /** Obtains the iteration count. */
    ErrorOr<UInt> iterations() const;

    /** Obtains the key derivation function (KDF) identifier. */
    ErrorOr<String> kdf() const;

    /** Obtains an optional KDF memory cost factor integer. */
    ErrorOr<UInt> memory() const;

    /** Sends an `AUTHENTICATE` message back in response to the challenge. */
    ErrorOrDone authenticate(Authentication auth);

    /** Thread-safe authenticate. */
    std::future<ErrorOrDone> authenticate(ThreadSafe, Authentication auth);

    /** Sends an `ABORT` message back in response to an invalid challenge. */
    ErrorOrDone fail(Reason reason);

    /** Thread-safe fail. */
    std::future<ErrorOrDone> fail(ThreadSafe, Reason reason);

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

public:
    // Internal use only
    using ChallengeePtr = std::weak_ptr<internal::Challengee>;
    Challenge(internal::PassKey, ChallengeePtr challengee,
              internal::ChallengeMessage&& msg);

private:
    using Base = Options<Challenge, internal::ChallengeMessage>;

    ChallengeePtr challengee_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/sessioninfo.ipp"
#endif

#endif // CPPWAMP_PEERDATA_HPP
