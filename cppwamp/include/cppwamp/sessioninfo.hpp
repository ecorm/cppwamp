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
#include "tagtypes.hpp"
#include "variantdefs.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"

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
class CPPWAMP_API Reason
    : public Options<Reason, internal::MessageKind::goodbye>
{
public:
    /** Converting constructor taking an optional reason URI. */
    Reason(Uri uri = {});

    /** Converting constructor taking an error code, attempting to convert
        it to a URI. */
    Reason(std::error_code ec);

    /** Converting constructor taking a WampErrc, attempting to convert
        it to a reason URI. */
    Reason(WampErrc errc);

    /** Sets the `message` member of the details dictionary. */
    Reason& withHint(String message);

    /** Obtains the reason URI. */
    const Uri& uri() const;

    /** Obtains the `message` member of the details dictionary. */
    ErrorOr<String> hint() const;

    /** Attempts to convert the reason URI to a known error code. */
    WampErrc errorCode() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool isServer) const;

private:
    static constexpr unsigned uriPos_ = 2;

    using Base = Options<Reason, internal::MessageKind::goodbye>;

public:
    // Internal use only
    Reason(internal::PassKey, internal::Message&& msg);
    void setUri(internal::PassKey, Uri uri);
    void setKindToAbort(internal::PassKey);
};


//------------------------------------------------------------------------------
/** %Realm URI and other options contained within WAMP `HELLO` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Realm : public Options<Realm, internal::MessageKind::hello>
{
public:
    /** Converting constructor taking a realm URI. */
    Realm(Uri uri);

    /** Specifies the Reason object in which to store abort details returned
        by the router. */
    Realm& captureAbort(Reason& reason);

    /** Obtains the realm URI. */
    const Uri& uri() const;

    /** Obtains the agent string. */
    ErrorOr<String> agent() const;

    /** Obtains the roles dictionary. */
    ErrorOr<Object> roles() const;

    /** Obtains the supported features. */
    ClientFeatures features() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool = false) const;

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
    static constexpr unsigned uriPos_     = 1;

    using Base = Options<Realm, internal::MessageKind::hello>;

    Reason* abortReason_ = nullptr;

public:
    // Internal use only
    Realm(internal::PassKey, internal::Message&& msg);
    Reason* abortReason(internal::PassKey);
    Uri& uri(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Session information contained within WAMP `WELCOME` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Welcome
    : public Options<Welcome, internal::MessageKind::welcome>
{
public:
    /** Default constructor. */
    Welcome();

    /** Obtains the WAMP session ID. */
    SessionId id() const;

    /** Obtains realm URI. */
    const Uri& realm() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool = false) const;

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
    RouterFeatures features() const;
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
    static constexpr unsigned sessionIdPos_ = 1;

    using Base = Options<Welcome, internal::MessageKind::welcome>;

    static RouterFeatures parseFeatures(const Object& opts);

    Uri realm_;
    RouterFeatures features_;

public:
    // Internal use only
    Welcome(internal::PassKey, Uri&& realm, internal::Message&& msg);
    Welcome(internal::PassKey, SessionId sid, Object&& opts);
};


//------------------------------------------------------------------------------
/** Provides the _Signature_ and _Extra_ dictionary contained within
    WAMP `AUTHENTICATE` messages.

    See [Authentication Methods in the WAMP specification]
    (https://wamp-proto.org/wamp_latest_ietf.html#name-authentication-methods) */
//------------------------------------------------------------------------------
class CPPWAMP_API Authentication
    : public Options<Authentication, internal::MessageKind::authenticate>
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
    AccessActionInfo info(bool = false) const;

private:
    static constexpr unsigned signaturePos_ = 1;

    using Base = Options<Authentication, internal::MessageKind::authenticate>;

public:
    // Internal use only
    Authentication(internal::PassKey, internal::Message&& msg);
};



namespace internal { class Challengee; } // Forward declaration

//------------------------------------------------------------------------------
/** Provides the _AuthMethod_ and _Extra_ dictionary contained within
    WAMP `CHALLENGE` messages.

    See [Authentication Methods in the WAMP specification]
    (https://wamp-proto.org/wamp_latest_ietf.html#name-authentication-methods) */
//------------------------------------------------------------------------------
class CPPWAMP_API Challenge
    : public Options<Challenge, internal::MessageKind::challenge>
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
    AccessActionInfo info(bool = false) const;

public:
    // Internal use only
    using ChallengeePtr = std::weak_ptr<internal::Challengee>;
    Challenge(internal::PassKey, ChallengeePtr challengee,
              internal::Message&& msg);

private:
    static constexpr unsigned authMethodPos_ = 1;

    using Base = Options<Challenge, internal::MessageKind::challenge>;

    ChallengeePtr challengee_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/sessioninfo.ipp"
#endif

#endif // CPPWAMP_PEERDATA_HPP
