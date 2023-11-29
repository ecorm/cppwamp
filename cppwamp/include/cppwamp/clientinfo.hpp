/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CLIENTINFO_HPP
#define CPPWAMP_CLIENTINFO_HPP

#include <string>
#include <vector>
#include <utility>
#include "accesslogging.hpp"
#include "api.hpp"
#include "errorcodes.hpp"
#include "errorinfo.hpp"
#include "erroror.hpp"
#include "exceptions.hpp"
#include "features.hpp"
#include "logging.hpp"
#include "options.hpp"
#include "timeout.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "internal/clientcontext.hpp"
#include "internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Provides data structures for information exchanged via client
           session management messages. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides the _reason_ URI and other options contained within
    `GOODBYE` and `ABORT` messages.*/
//------------------------------------------------------------------------------
class CPPWAMP_API Goodbye
    : public Options<Goodbye, internal::MessageKind::goodbye>
{
public:
    /// URI character type
    using UriChar = Uri::value_type;

    // NOLINTBEGIN(google-explicit-constructor)

    /** Default constructor. */
    Goodbye();

    /** Converting constructor taking a reason URI. */
    Goodbye(Uri uri);

    /** Converting constructor taking a reason URI C string. */
    Goodbye(const UriChar* uri);

    /** Converting constructor taking an error code, attempting to convert
        it to a URI. */
    Goodbye(std::error_code ec);

    /** Converting constructor taking a WampErrc, attempting to convert
        it to a reason URI. */
    Goodbye(WampErrc errc);

    // NOLINTEND(google-explicit-constructor)

    /** Obtains the reason URI. */
    const Uri& uri() const &;

    /** Moves the reason URI. */
    Uri&& uri() &&;

    /** Attempts to convert the reason URI to a known error code. */
    WampErrc errorCode() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool isServer) const;

private:
    static constexpr unsigned uriPos_ = 2;

    using Base = Options<Goodbye, internal::MessageKind::goodbye>;

public:
    // Internal use only
    Goodbye(internal::PassKey, internal::Message&& msg);
    void setUri(internal::PassKey, Uri uri);
};


//------------------------------------------------------------------------------
/** Provides the _reason_ URI, options, and payload arguments contained
    within WAMP `ABORT` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Abort : public Payload<Abort, internal::MessageKind::abort>
{
public:
    /** Default constructor. */
    Abort();

    // NOLINTBEGIN(google-explicit-constructor)

    /** Converting constructor taking a reason URI and optional positional
        payload arguments. */
    template <typename... Ts>
    Abort(Uri uri, Ts&&... args)
        : Abort(in_place, std::move(uri), Array{std::forward<Ts>(args)...})
    {}

    /** Converting constructor taking an error code, attempting to convert
        it to a reason URI, as well as optional positional payload arguments. */
    template <typename... Ts>
    Abort(std::error_code e, Ts&&... args)
        : Abort(in_place, errorCodeToUri(e), Array{std::forward<Ts>(args)...})
    {}

    /** Converting constructor taking a WampErrc, attempting to convert
        it to a reason URI, as well as optional positional payload arguments. */
    template <typename... Ts>
    Abort(WampErrc e, Ts&&... args)
        : Abort(in_place, errorCodeToUri(e), Array{std::forward<Ts>(args)...})
    {}

    // NOLINTEND(google-explicit-constructor)

    /** Constructor taking an error::BadType exception and
        interpreting it as a `wamp.error.invalid_argument` reason URI. */
    explicit Abort(const error::BadType& e);

    /** Copy constructor. */
    Abort(const Abort&) = default;

    /** Move constructor. */
    Abort(Abort&&) = default;

    /** Destructor. */
    virtual ~Abort() = default;

    /** Copy assignment. */
    Abort& operator=(const Abort&) = default;

    /** Move assignment. */
    Abort& operator=(Abort&&) = default;

    /** Sets the `message` member of the details dictionary. */
    Abort& withHint(String text);

    /** Conversion to bool operator, returning false if the error is empty. */
    explicit operator bool() const;

    /** Obtains the reason URI. */
    const Uri& uri() const &;

    /** Moves the reason URI. */
    Uri&& uri() &&;

    /** Obtains the `message` member of the details dictionary. */
    ErrorOr<String> hint() const &;

    /** Moves the `message` member of the details dictionary. */
    ErrorOr<String> hint() &&;

    /** Attempts to convert the reason URI to a known error code. */
    WampErrc errorCode() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool isServer) const;

private:
    static constexpr unsigned uriPos_ = 2;

    using Base = Payload<Abort, internal::MessageKind::abort>;

    explicit Abort(in_place_t, Uri uri, Array args);

public: // Internal use only
    Abort(internal::PassKey, internal::Message&& msg);

    Abort(internal::PassKey, WampErrc errc, Object opts = {});

    Abort(internal::PassKey, std::error_code ec, Object opts = {});
};


//------------------------------------------------------------------------------
/** %Realm URI and other options contained within WAMP `HELLO` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Hello : public Options<Hello, internal::MessageKind::hello>
{
public:
    /// URI character type
    using UriChar = Uri::value_type;

    /** Converting constructor taking a realm URI. */
    Hello(Uri realm); // NOLINT(google-explicit-constructor)

    /** Converting constructor taking a realm URI C string. */
    Hello(const UriChar* realm); // NOLINT(google-explicit-constructor)

    /** Specifies the Goodbye object in which to store abort details returned
        by the router. */
    Hello& captureAbort(Abort& reason);

    /** Specifies the duration after which the joining operation should time out
        and disconnect the session. */
    Hello& withTimeout(Timeout timeout);

    /** Specifies the agent string to use. */
    Hello& withAgent(String agent);

    /** Obtains the joining operation timeout duration. */
    Timeout timeout() const;

    /** Obtains the realm URI. */
    const Uri& uri() const;

    /** Obtains the agent string. */
    ErrorOr<String> agent() const;

    /** Obtains the roles dictionary. */
    ErrorOr<Object> roles() const;

    /** Obtains the supported features. */
    ClientFeatures features() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Authentication
        See [Authentication Methods in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-authentication-methods)
        @{ */

    /** Sets the `HELLO.Details.authmethods` option. */
    Hello& withAuthMethods(std::vector<String> methods);

    /** Sets the `HELLO.Details.authid` option. */
    Hello& withAuthId(String authId);

    /** Obtains the `authmethods` array. */
    ErrorOr<Array> authMethods() const;

    /** Obtains the `authid` string, or an empty string if unavailable. */
    ErrorOr<String> authId() const;
    /// @}

private:
    static constexpr unsigned uriPos_ = 1;

    using Base = Options<Hello, internal::MessageKind::hello>;

    Abort* abortReason_ = nullptr;
    Timeout timeout_ = unspecifiedTimeout;

public:
    // Internal use only
    Hello(internal::PassKey, internal::Message&& msg);
    Abort* abortReason(internal::PassKey);
    Uri& uri(internal::PassKey);
    String agentOrEmptyString(internal::PassKey);
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
    SessionId sessionId() const;

    /** Obtains realm URI. */
    const Uri& realm() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Agent Identification
        See [Agent Identification in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-hello-2)
        @{ */

    /** Obtains the agent string of the WAMP router. */
    ErrorOr<String> clientAgentString() const;
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
    Welcome(internal::PassKey, internal::Message&& msg);
    Welcome(internal::PassKey, SessionId sid, Object&& opts);
    void setRealm(internal::PassKey, Uri&& realm);
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
    Authentication(String signature); // NOLINT(google-explicit-constructor)

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
    static constexpr unsigned signaturePos_ = 1;

    using Base = Options<Authentication, internal::MessageKind::authenticate>;

public:
    // Internal use only
    Authentication(internal::PassKey, internal::Message&& msg);
};


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
    explicit Challenge(String authMethod = {});

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
    void authenticate(Authentication auth);

    /** Sends an `ABORT` message back in response to an invalid challenge. */
    void fail(Abort reason);

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

private:
    using Base = Options<Challenge, internal::MessageKind::challenge>;
    using Context = internal::ClientContext;

    static constexpr unsigned authMethodPos_ = 1;

    internal::ClientContext challengee_;

public:
    // Internal use only
    Challenge(internal::PassKey, internal::Message&& msg);
    void setChallengee(internal::PassKey, Context challengee);
};

//------------------------------------------------------------------------------
/** Enumerates spontaneous Session event types.

    One of the following error codes is emitted alongside IncidentKind::trouble:

    Error Code                    | Cause
    ----------------------------- | -----
    WampErrc::payloadSizeExceeded | Outbound RESULT/ERROR exceeded transport limits
    WampErrc::noSuchProcedure     | No registration matched URI of received INVOCATION
    MiscErrc::noSuchTopic         | No subscription matched URI of received EVENT

    @note `transportDropped` may follow `closedByPeer` or `abortedByPeer`
          depending on the behavior of the router. */
//------------------------------------------------------------------------------
enum class IncidentKind
{
    transportDropped, ///< Transport connection dropped by peer or network.
    closedByPeer,     ///< WAMP session killed by the remote peer.
    abortedByPeer,    ///< WAMP session aborted by the remote peer.
    commFailure,      ///< A fatal transport or protocol error occurred.
    challengeFailure, ///< The challenge handler reported an error.
    eventError,       ///< A pub-sub event handler reported an error.
    unknownErrorUri,  ///< An ERROR with unknown URI was received.
    errorHasPayload,  ///< An ERROR with payload arguments was received.
    trouble,          ///< A non-fatal problem occurred.
    trace,            ///< A WAMP message was sent or received.
    count
};

//------------------------------------------------------------------------------
/** Obtains a description string for the given IncidentKind. */
//------------------------------------------------------------------------------
CPPWAMP_API const std::string& incidentDescription(IncidentKind k);

//------------------------------------------------------------------------------
/** Contains information on a spontanous Session event. */
//------------------------------------------------------------------------------
class CPPWAMP_API Incident
{
public:
    Incident( // NOLINT(google-explicit-constructor)
        IncidentKind kind, std::string msg = {});

    Incident(IncidentKind kind, std::error_code ec, std::string msg = {});

    Incident(IncidentKind kind, const Goodbye& g);

    Incident(IncidentKind kind, const Abort& reason);

    Incident(IncidentKind kind, const Error& e);

    /** Obtains the type of incident. */
    IncidentKind kind() const;

    /** Optional error code associated with the incident. */
    std::error_code error() const;

    /** Obtains optional additional information. */
    std::string message() const;

    /** Generates a LogEntry for the incident. */
    LogEntry toLogEntry() const;

private:
    std::string message_;   ///< Provides optional additional information.
    std::error_code error_; ///< Error code associated with the incident.
    IncidentKind kind_;     ///< The type of incident.
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/clientinfo.inl.hpp"
#endif

#endif // CPPWAMP_CLIENTINFO_HPP
