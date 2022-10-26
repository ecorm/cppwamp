/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PEERDATA_HPP
#define CPPWAMP_PEERDATA_HPP

#include <cassert>
#include <chrono>
#include <future>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "api.hpp"
#include "anyhandler.hpp"
#include "config.hpp"
#include "erroror.hpp"
#include "options.hpp"
#include "payload.hpp"
#include "tagtypes.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains declarations for data structures exchanged between
           WAMP peers. */
//------------------------------------------------------------------------------

// TODO: Use name-based WAMP spec links

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides the _reason_ URI and other options contained within
    `ABORT` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Abort : public Options<Abort, internal::AbortMessage>
{
public:
    /** Converting constructor taking an optional reason URI. */
    Abort(String uri = "");

    /** Obtains the reason URI. */
    const String& uri() const;

private:
    using Base = Options<Abort, internal::AbortMessage>;

public:
    // Internal use only
    Abort(internal::PassKey, internal::AbortMessage&& msg);
};

//------------------------------------------------------------------------------
/** %Realm URI and other options contained within WAMP `HELLO` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Realm : public Options<Realm, internal::HelloMessage>
{
public:
    /** Converting constructor taking a realm URI. */
    Realm(String uri);

    /** Obtains the realm URI. */
    const String& uri() const;

    /** Specifies the Abort object in which to store abort details returned
        by the router. */
    Realm& captureAbort(Abort& abort);

    /** @name Authentication
        See [Authentication in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.5.2)
        @{ */

    /** Sets the `HELLO.Details.authmethods` option. */
    Realm& withAuthMethods(std::vector<String> methods);

    /** Sets the `HELLO.Details.authid` option. */
    Realm& withAuthId(String authId);

    /** Obtains the `authmethods` array, or an empty array if absent. */
    Array authMethods() const;

    /** Obtains the `authid` string, or an empty string if absent. */
    String authId() const;
    /// @}

private:
    using Base = Options<Realm, internal::HelloMessage>;

    Abort* abort_ = nullptr;

public:
    // Internal use only
    Realm(internal::PassKey, internal::HelloMessage&& msg);
    Abort* abort(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Session information contained within WAMP `WELCOME` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API SessionInfo : public Options<SessionInfo,
                                               internal::WelcomeMessage>
{
public:
    /** A set of role strings. */
    using RoleSet = std::set<String>;

    /** A set of feature strings. */
    using FeatureSet = std::set<String>;

    /** A dictionary of feature sets to be supported by each role. */
    using FeatureMap = std::map<String, FeatureSet>;

    /** Default constructor. */
    SessionInfo();

    /** Obtains the WAMP session ID. */
    SessionId id() const;

    /** Obtains realm URI. */
    const String& realm() const;

    /** @name Agent Identification
        See [Agent Identification in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.8)
        @{ */

    /** Obtains the agent string of the WAMP router, if available. */
    String agentString() const;
    /// @}

    /** @name Role and Feature Announcement
        See [Client: Role and Feature Announcement in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.7.1.1.1)
        @{ */

    /** Obtains the `WELCOME.Details.roles` dictionary for the router. */
    Object roles() const;

    /** Checks that the router supports the given set of roles. */
    bool supportsRoles(const RoleSet& roles) const;

    /** Checks that the router supports the given map of features. */
    bool supportsFeatures(const FeatureMap& features) const;
    /// @}

    /** @name Authentication
        See [Authentication in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.5.2)
        @{ */

    /** Obtains the authentication ID the client was actually
        authenticated as. */
    // TODO: Return string instead that can be empty if field is absent
    Variant authId() const;

    /** Obtains the role the client was authenticated for. */
    Variant authRole() const;

    /** Obtains the method that was used for authentication. */
    Variant authMethod() const;

    /** Obtains the authentication provider. */
    Variant authProvider() const;

    /** Obtains extra authentication details. */
    Variant authExtra() const;
    /// @}

private:
    using Base = Options<SessionInfo, internal::WelcomeMessage>;

    String realm_;

public:
    // Internal use only
    SessionInfo(internal::PassKey, String&& realm,
                internal::WelcomeMessage&& msg);
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out,
                                     const SessionInfo& info);


//------------------------------------------------------------------------------
/** Provides the _reason_ URI and other options contained within
    `GOODBYE` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Reason : public Options<Reason, internal::GoodbyeMessage>
{
public:
    /** Converting constructor taking an optional reason URI. */
    Reason(String uri = "");

    /** Obtains the reason URI. */
    const String& uri() const;

private:
    using Base = Options<Reason, internal::GoodbyeMessage>;

public:
    // Internal use only
    Reason(internal::PassKey, internal::GoodbyeMessage&& msg);
};

//------------------------------------------------------------------------------
/** Provides the _Signature_ and _Extra_ dictionary contained within
    WAMP `AUTHENTICATE` messages.

    See [Authentication in the WAMP specification]
    (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.5.2) */
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

private:
    using Base = Options<Authentication, internal::AuthenticateMessage>;
};


namespace internal { class Challengee; } // Forward declaration

//------------------------------------------------------------------------------
/** Provides the _AuthMethod_ and _Extra_ dictionary contained within
    WAMP `CHALLENGE` messages.

    See [Authentication in the WAMP specification]
    (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.5.2) */
//------------------------------------------------------------------------------
class CPPWAMP_API Challenge : public Options<Challenge,
                                             internal::ChallengeMessage>
{
public:
    /** Constructs an empty challenge. */
    Challenge();

    /** Determines if the Session object that dispatched this
        invocation still exists or has expired. */
    bool challengeeHasExpired() const;

    /** Obtains the authentication method string. */
    const String& method() const;

    /** Returns an optional challenge string. */
    Variant challenge() const;

    /** Returns an optional salt string. */
    Variant salt() const;

    /** Returns an optional key length. */
    Variant keyLength() const;

    /** Returns an optional iteration count. */
    Variant iterations() const;

    /** Returns an optional key derivation function (KDF) identifier. */
    Variant kdf() const;

    /** Returns an optional KDF memory cost factor integer. */
    Variant memory() const;

    /** Sends an `AUTHENTICATE` message back in response to the challenge. */
    ErrorOrDone authenticate(Authentication auth);

    /** Thread-safe authenticate. */
    std::future<ErrorOrDone> authenticate(ThreadSafe, Authentication auth);

public:
    // Internal use only
    using ChallengeePtr = std::weak_ptr<internal::Challengee>;
    Challenge(internal::PassKey, ChallengeePtr challengee,
              internal::ChallengeMessage&& msg);

private:
    using Base = Options<Challenge, internal::ChallengeMessage>;

    ChallengeePtr challengee_;
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
    const Authentication& authentication() const;
    unsigned stage() const;
    const Variant& memento() const;

    void challenge(Challenge challenge, Variant memento = {});
    void challenge(ThreadSafe, Challenge challenge, Variant memento = {});
    void welcome(Object details);
    void welcome(ThreadSafe, Object details);
    void abort(Object details = {});
    void abort(ThreadSafe, Object details = {});

public:
    // Internal use only
    using ChallengerPtr = std::weak_ptr<internal::Challenger>;
    static Ptr create(internal::PassKey, Realm&& r, ChallengerPtr c);

private:
    AuthExchange(Realm&& r, ChallengerPtr c);

    Realm realm_;
    ChallengerPtr challenger_;
    Authentication authentication_;
    Variant memento_; // Useful for keeping the authorizer stateless
    unsigned stage_;
};


//------------------------------------------------------------------------------
/** Provides the _reason_ URI, options, and payload arguments contained
    within WAMP `ERROR` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Error : public Payload<Error, internal::ErrorMessage>
{
public:
    /** Constructs an empty error. */
    Error();

    /** Converting constructor taking a reason URI. */
    Error(String reason);

    /** Constructor taking an error::BadType exception and
        interpreting it as a `wamp.error.invalid_argument` reason URI. */
    explicit Error(const error::BadType& e);

    /** Destructor. */
    virtual ~Error();

    /** Conversion to bool operator, returning false if the error is empty. */
    explicit operator bool() const;

    /** Obtains the reason URI. */
    const String& reason() const;

private:
    using Base = Payload<Error, internal::ErrorMessage>;

public:
    // Internal use only
    Error(internal::PassKey, internal::ErrorMessage&& msg);

    internal::ErrorMessage& errorMessage(internal::PassKey,
                                         internal::WampMsgType reqType,
                                         RequestId reqId);
};


//------------------------------------------------------------------------------
/** Provides the topic URI and other options contained within WAMP
    `SUBSCRIBE' messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Topic : public Options<Topic, internal::SubscribeMessage>
{
public:
    /** Converting constructor taking a topic URI. */
    Topic(String uri);

    /** @name Pattern-based Subscriptions
        See [Pattern-based Subscriptions in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.6)
        @{ */

    /** Specifies that the _prefix-matching policy_ is to be used for this
        subscription. */
    Topic& usingPrefixMatch();

    /** Specifies that the _wildcard-matching policy_ is to be used for this
        subscription. */
    Topic& usingWildcardMatch();
    /// @}

    /** Obtains the topic URI. */
    const String& uri() const;

private:
    using Base = Options<Topic, internal::SubscribeMessage>;
};


//------------------------------------------------------------------------------
/** Provides the topic URI, options, and payload contained within WAMP
    `PUBLISH` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Pub : public Payload<Pub, internal::PublishMessage>
{
public:
    /** Converting constructor taking a topic URI. */
    Pub(String topic);

    /** @name Subscriber Allow/Deny Lists
        See [Subscriber Black- and Whitelisting in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.1)
        @{ */

    /** Obtains the topic URI. */
    const String& topic();

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        won't receive the published event. */
    Pub& withExcludedSessions(Array sessionIds);

    /** Specifies a deny list of authid strings. */
    Pub& withExcludedAuthIds(Array authIds);

    /** Specifies a deny list of authrole strings. */
    Pub& withExcludedAuthRoles(Array authRoles);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        are allowed to receive the published event. */
    Pub& withEligibleSessions(Array sessionIds);

    /** Specifies an allow list of authid strings. */
    Pub& withEligibleAuthIds(Array authIds);

    /** Specifies an allow list of authrole strings. */
    Pub& withEligibleAuthRoles(Array authRoles);
    /// @}

    /** @name Publisher Exclusion
        See [Publisher Exclusion in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.2)
        @{ */

    /** Specifies if this session should be excluded from receiving the
        event. */
    Pub& withExcludeMe(bool excluded = true);
    /// @}

    /** @name Publisher Identification
        See [Publisher Identification in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.3)
        @{ */

    /** Requests that the identity (session ID) of this session be disclosed
        in the event. */
    Pub& withDiscloseMe(bool disclosed = true);
    /// @}

private:
    using Base = Payload<Pub, internal::PublishMessage>;
};


//------------------------------------------------------------------------------
/** Provides the subscription/publication ids, options, and payload contained
    within WAMP `EVENT` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Event : public Payload<Event, internal::EventMessage>
{
public:
    /** Default constructor. */
    Event();

    /** Returns `false` if the Event has been initialized and is ready
        for use. */
    bool empty() const;

    /** Obtains the subscription ID associated with this event. */
    SubscriptionId subId() const;

    /** Obtains the publication ID associated with this event. */
    PublicationId pubId() const;

    /** Obtains the executor used to execute user-provided handlers. */
    AnyCompletionExecutor executor() const;

    /** @name Publisher Identification
        See [Publisher Identification in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.3)
        @{ */

    /** Obtains an optional publisher ID integer. */
    Variant publisher() const;
    /// @}

    /** @name Publication Trust Levels
        See [Publication Trust Levels in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.4)
        @{ */

    /** Obtains an optional trust level integer. */
    Variant trustLevel() const;
    /// @}

    /** @name Pattern-based Subscriptions
        See [Pattern-based Subscriptions in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.6)
        @{ */

    /** Obtains an optional string of the original topic URI used to make the
        publication. */
    Variant topic() const;
    /// @}

private:
    using Base = Payload<Event, internal::EventMessage>;

    AnyCompletionExecutor executor_;

public:
    // Internal use only
    Event(internal::PassKey, AnyCompletionExecutor executor,
          internal::EventMessage&& msg);
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Event& event);


//------------------------------------------------------------------------------
/** Contains the procedure URI and other options contained within
    WAMP `REGISTER` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Procedure : public Options<Procedure,
                                             internal::RegisterMessage>
{
public:
    /** Converting constructor taking a procedure URI. */
    Procedure(String uri);

    /** Obtains the procedure URI. */
    const String& uri() const;

    /** @name Pattern-based Registrations
        See [Pattern-based Registrations in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.8)
        @{ */

    /** Specifies that the _prefix-matching policy_ is to be used for this
        registration. */
    Procedure& usingPrefixMatch();

    /** Specifies that the _wildcard-matching policy_ is to be used for this
        subscription. */
    Procedure& usingWildcardMatch();
    /// @}

    /** @name Caller Identification
        See [Caller Identification in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.5)
        @{ */

    /** Requests that the identity (session ID) of this session be disclosed
        in the remote procedure call. */
    Procedure& withDiscloseCaller(bool disclosed = true);
    /// @}

private:
    using Base = Options<Procedure, internal::RegisterMessage>;
};


//------------------------------------------------------------------------------
/** Contains the procedure URI, options, and payload contained within
    WAMP `CALL` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Rpc : public Payload<Rpc, internal::CallMessage>
{
public:
    /** The duration type used for caller-initiated timeouts. */
    using CallerTimeoutDuration = std::chrono::steady_clock::duration;

    static constexpr CallCancelMode defaultCancelMode() noexcept
    {
        return CallCancelMode::kill;
    }

    /** Converting constructor taking a procedure URI. */
    Rpc(String uri);

    /** Obtains the procedure URI. */
    const String& procedure() const;

    /** Specifies the Error object in which to store call errors returned
        by the callee. */
    Rpc& captureError(Error& error);

    /** @name Progressive Call Results
        See [Progressive Call Results in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.1)
        @{ */

    /** Sets willingness to receive progressive results. */
    Rpc& withProgressiveResults(bool enabled = true);

    /** Indicates if progressive results were enabled. */
    bool progressiveResultsAreEnabled() const;
    /// @}

    /** @name Call Timeouts
        See [Call Timeouts in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.3).
        Setting a duration of zero deactivates the timeout.
        @{ */

    /** Requests that the dealer cancel the call after the specified
        timeout duration in milliseconds. */
    Rpc& withDealerTimeout(UInt milliseconds);

    /** Requests that the dealer cancel the call after the specified
        timeout duration. */
    template <typename R, typename P>
    Rpc& withDealerTimeout(std::chrono::duration<R, P> timeout)
    {
        using namespace std::chrono;
        auto ms = duration_cast<milliseconds>(timeout).count();
        return withDealerTimeout(static_cast<Int>(ms));
    }

    /** Requests that the caller cancel the call after the specified
        timeout duration in milliseconds. */
    Rpc& withCallerTimeout(UInt milliseconds);

    /** Requests that the dealer cancel the call after the specified
        timeout duration. */
    template <typename R, typename P>
    Rpc& withCallerTimeout(std::chrono::duration<R, P> timeout)
    {
        using namespace std::chrono;
        setCallerTimeout(duration_cast<CallerTimeoutDuration>(timeout));
        return *this;
    }

    /** Obtains the caller timeout duration. */
    CallerTimeoutDuration callerTimeout() const;

    /// @}

    /** @name Caller Identification
        See [Caller Identification in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.5)
        @{ */

    /** Requests that the identity (session ID) of this session be disclosed
        in the call invocation. */
    Rpc& withDiscloseMe(bool disclosed = true);
    /// @}

    /** @name Call Cancellation
        @{ */

    /** Sets the default cancellation mode to use when none is specified. */
    Rpc& withCancelMode(CallCancelMode mode);

    /** Obtains the default cancellation mode associated with this RPC. */
    CallCancelMode cancelMode() const;
    /// @}

private:
    using Base = Payload<Rpc, internal::CallMessage>;

    void setCallerTimeout(CallerTimeoutDuration duration);

    Error* error_ = nullptr;
    CallerTimeoutDuration callerTimeout_ = {};
    CallCancelMode cancelMode_ = defaultCancelMode();
    bool progressiveResultsEnabled_ = false;

public:
    Error* error(internal::PassKey); // Internal use only
};


//------------------------------------------------------------------------------
/** Contains the remote procedure result options/payload within WAMP
    `RESULT` and `YIELD` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Result : public Payload<Result, internal::ResultMessage>
{
public:
    /** Default constructor. */
    Result();

    /** Converting constructor taking a braced initializer list of
        positional arguments. */
    Result(std::initializer_list<Variant> list);

    /** Obtains the request ID associated with the call. */
    RequestId requestId() const;

    /** @name Progressive Call Results
        See [Progressive Call Results in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.1)
        @{ */

    /** Lets the callee specify if the yielded result is progressive. */
    Result& withProgress(bool progressive = true);

    /** Indicates if the result is progressive. */
    bool isProgressive() const;
    /// @}

private:
    using Base = Payload<Result, internal::ResultMessage>;
    Result(RequestId reqId, Object&& details);

public:
    // Internal use only
    Result(internal::PassKey, internal::ResultMessage&& msg);

    Result(internal::PassKey, internal::YieldMessage&& msg);

    internal::YieldMessage& yieldMessage(internal::PassKey, RequestId reqId)
    {
        message().setRequestId(reqId);
        return message().transformToYield();
    }
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Result& result);


//------------------------------------------------------------------------------
/** Tag type that can be passed to wamp::Outcome to construct a
    deferred outcome.
    Use the wamp::deferment constant object to more conveniently pass this tag. */
//------------------------------------------------------------------------------
struct Deferment
{
    constexpr Deferment() noexcept = default;
};

//------------------------------------------------------------------------------
/** Convenient value of the wamp::Deferment tag type that can be passed to
    the wamp::Outcome constructor. */
//------------------------------------------------------------------------------
constexpr CPPWAMP_INLINE_VARIABLE Deferment deferment;

//------------------------------------------------------------------------------
/** Contains the outcome of an RPC invocation.
    @see @ref RpcOutcomes */
//------------------------------------------------------------------------------
class CPPWAMP_API Outcome
{
public:
    /** Enumerators representing the type of outcome being held by
        this object. */
    enum class Type
    {
        deferred, ///< A `YIELD` has been, or will be, sent manually.
        result,   ///< Contains a wamp::Result to be yielded back to the caller.
        error     ///< Contains a wamp::Error to be yielded back to the caller.
    };

    /** Default-constructs an outcome containing an empty Result object. */
    Outcome();

    /** Converting constructor taking a Result object. */
    Outcome(Result result);

    /** Converting constructor taking a braced initializer list of positional
        arguments to be stored in a Result. */
    Outcome(std::initializer_list<Variant> args);

    /** Converting constructor taking an Error object. */
    Outcome(Error error);

    /** Converting constructor taking a deferment. */
    Outcome(Deferment);

    /** Copy constructor. */
    Outcome(const Outcome& other);

    /** Move constructor. */
    Outcome(Outcome&& other);

    /** Destructor. */
    ~Outcome();

    /** Obtains the object type being contained. */
    Type type() const;

    /** Accesses the stored Result object. */
    const Result& asResult() const &;

    /** Steals the stored Result object. */
    Result&& asResult() &&;

    /** Accesses the stored Error object. */
    const Error& asError() const &;

    /** Steals the stored Error object. */
    Error&& asError() &&;

    /** Copy-assignment operator. */
    Outcome& operator=(const Outcome& other);

    /** Move-assignment operator. */
    Outcome& operator=(Outcome&& other);

private:
    CPPWAMP_HIDDEN explicit Outcome(std::nullptr_t);
    CPPWAMP_HIDDEN void copyFrom(const Outcome& other);
    CPPWAMP_HIDDEN void moveFrom(Outcome&& other);
    CPPWAMP_HIDDEN void destruct();

    Type type_;

    union CPPWAMP_HIDDEN Value
    {
        Value() {}
        ~Value() {}

        Result result;
        Error error;
    } value_;
};


namespace internal { class Callee; } // Forward declaration

//------------------------------------------------------------------------------
/** Contains payload arguments and other options within WAMP `INVOCATION`
    messages.

    This class also provides the means for manually sending a `YIELD` or
    `ERROR` result back to the RPC caller. */
//------------------------------------------------------------------------------
class CPPWAMP_API Invocation : public Payload<Invocation,
                                              internal::InvocationMessage>
{
public:
    /** Default constructor */
    Invocation();

    /** Returns `false` if the Invocation has been initialized and is ready
        for use. */
    bool empty() const;

    /** Determines if the Session object that dispatched this
        invocation still exists or has expired. */
    bool calleeHasExpired() const;

    /** Returns the request ID associated with this RPC invocation. */
    RequestId requestId() const;

    /** Obtains the executor used to execute user-provided handlers. */
    AnyCompletionExecutor executor() const;

    /** Manually sends a `YIELD` result back to the callee. */
    ErrorOrDone yield(Result result = Result()) const;

    /** Thread-safe yield result. */
    std::future<ErrorOrDone> yield(ThreadSafe, Result result = Result()) const;

    /** Manually sends an `ERROR` result back to the callee. */
    ErrorOrDone yield(Error error) const;

    /** Thread-safe yield error. */
    std::future<ErrorOrDone> yield(ThreadSafe, Error error) const;

    /** @name Progressive Call Results
        See [Progressive Call Results in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.1)
        @{ */

    /** Checks if the caller requested progressive results. */
    bool isProgressive() const;
    /// @}

    /** @name Caller Identification
        See [Caller Identification in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.5)
        @{ */

    /** Returns an optional session ID integer of the caller. */
    Variant caller() const;
    /// @}

    /** @name Call Trust Levels
        See [Call Trust Levels in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.6)
        @{ */

    /** Returns an optional trust level integer. */
    Variant trustLevel() const;
    /// @}

    /** @name Pattern-based Registrations
        See [Pattern-based Registrations in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.8)
        @{ */

    /** Returns an optional string of the original procedure URI used to make
        this call. */
    Variant procedure() const;
    /// @}

public:
    // Internal use only
    using CalleePtr = std::weak_ptr<internal::Callee>;
    Invocation(internal::PassKey, CalleePtr callee,
               AnyCompletionExecutor executor,
               internal::InvocationMessage&& msg);

private:
    using Base = Payload<Invocation, internal::InvocationMessage>;

    CalleePtr callee_;
    AnyCompletionExecutor executor_ = nullptr;

    template <typename, typename...> friend class CoroInvocationUnpacker;
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Invocation& inv);


//------------------------------------------------------------------------------
/** Contains the request ID and options contained within
    WAMP `CANCEL` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallCancellation
    : public Options<CallCancellation, internal::CancelMessage>
{
public:
    /** Converting constructor. */
    CallCancellation(RequestId reqId,
                     CallCancelMode cancelMode = Rpc::defaultCancelMode());

    /** Obtains the request ID of the call to cancel. */
    RequestId requestId() const;

    /** Obtains the cancel mode. */
    CallCancelMode mode() const;

private:
    using Base = Options<CallCancellation, internal::CancelMessage>;

    RequestId requestId_;
    CallCancelMode mode_;
};

//------------------------------------------------------------------------------
/** Contains details within WAMP `INTERRUPT` messages.

    This class also provides the means for manually sending a `YIELD` or
    `ERROR` result back to the RPC caller. */
//------------------------------------------------------------------------------
class CPPWAMP_API Interruption : public Options<Interruption,
                                                internal::InterruptMessage>
{
public:
    /** Default constructor */
    Interruption();

    /** Returns `false` if the Interruption has been initialized and is ready
        for use. */
    bool empty() const;

    /** Determines if the Session object that dispatched this
        interruption still exists or has expired. */
    bool calleeHasExpired() const;

    /** Returns the request ID associated with this interruption. */
    RequestId requestId() const;

    /** Obtains the executor used to execute user-provided handlers. */
    AnyCompletionExecutor executor() const;

    /** Manually sends a `YIELD` result back to the callee. */
    ErrorOrDone yield(Result result = Result()) const;

    /** Thread-safe yield result. */
    std::future<ErrorOrDone> yield(ThreadSafe, Result result = Result()) const;

    /** Manually sends an `ERROR` result back to the callee. */
    ErrorOrDone yield(Error error) const;

    /** Thread-safe yield error. */
    std::future<ErrorOrDone> yield(ThreadSafe, Error error) const;

public:
    // Internal use only
    using CalleePtr = std::weak_ptr<internal::Callee>;
    Interruption(internal::PassKey, CalleePtr callee,
                 AnyCompletionExecutor executor,
                 internal::InterruptMessage&& msg);

private:
    using Base = Options<Interruption, internal::InterruptMessage>;

    CalleePtr callee_;
    AnyCompletionExecutor executor_ = nullptr;
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out,
                                     const Interruption& cncltn);

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/peerdata.ipp"
#endif

#endif // CPPWAMP_PEERDATA_HPP
