/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSIONDATA_HPP
#define CPPWAMP_SESSIONDATA_HPP

#include <cassert>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include "api.hpp"
#include "asiodefs.hpp"
#include "peerdata.hpp"
#include "options.hpp"
#include "payload.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains declarations for data types exchanged via the client session
           interfaces. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** %Realm URI and other options passed to Session::join. */
//------------------------------------------------------------------------------
class CPPWAMP_API Realm : public Options<Realm>
{
public:
    /** Converting constructor taking a realm URI. */
    Realm(String uri);

    /** Obtains the realm URI. */
    const String& uri() const;

    /** @name Authentication
        See [Authentication in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.5.2)
        @{ */

    /** Sets the `HELLO.Details.authmethods` option. */
    Realm& withAuthMethods(std::vector<String> methods);

    /** Sets the `HELLO.Details.authid` option. */
    Realm& withAuthId(String authId);
    /// @}

private:
    String uri_;

public:
    String& uri(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Session information returned by Session::join. */
//------------------------------------------------------------------------------
class CPPWAMP_API SessionInfo : public Options<SessionInfo>
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
    Variant authId() const;

    /** Obtains the role the client was authenticated for. */
    Variant authRole() const;

    /** Obtains the method that was used for authentication. */
    Variant authMethod() const;

    /** Obtains the authentication provider. */
    Variant authProvider() const;
    /// @}

private:
    String realm_;
    SessionId sid_ = -1;

public:
    // Internal use only
    SessionInfo(internal::PassKey, String realm, SessionId id, Object details);
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out,
                                     const SessionInfo& info);

//------------------------------------------------------------------------------
/** %Topic URI and other options passed to Session::subscribe. */
//------------------------------------------------------------------------------
class CPPWAMP_API Topic : public Options<Topic>
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
    String uri_;

public:
    CPPWAMP_HIDDEN String& uri(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Contains the topic URI, options, and payload passed to Session::publish. */
//------------------------------------------------------------------------------
class CPPWAMP_API Pub : public Options<Pub>, public Payload<Pub>
{
public:
    /** Converting constructor taking a topic URI. */
    Pub(String topic);

    /** @name Subscriber Allow/Deny Lists
        See [Subscriber Black- and Whitelisting in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.4.1)
        @{ */

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
    String topic_;

public:
    String& topic(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Represents a published event */
//------------------------------------------------------------------------------
class CPPWAMP_API Event : public Options<Event>, public Payload<Event>
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
    AnyExecutor executor() const;

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
    SubscriptionId subId_ = -1;
    PublicationId pubId_  = -1;
    AnyExecutor executor_;

public:
    // Internal use only
    Event(internal::PassKey, SubscriptionId subId, PublicationId pubId,
          AnyExecutor executor, Object&& details);
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Event& event);

//------------------------------------------------------------------------------
/** %Procedure URI and other options passed to Session::enroll. */
//------------------------------------------------------------------------------
class CPPWAMP_API Procedure : public Options<Procedure>
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
    String uri_;

public:
    CPPWAMP_HIDDEN String& uri(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Contains the procedure URI, options, and payload passed to Session::call. */
//------------------------------------------------------------------------------
class CPPWAMP_API Rpc : public Options<Rpc>, public Payload<Rpc>
{
public:
    /** Converting constructor taking a procedure URI. */
    Rpc(String procedure);

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
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.3)
        @{ */

    /** Requests that the dealer cancels the call after the specified
        timeout duration. */
    Rpc& withDealerTimeout(Int milliseconds);
    /// @}

    /** @name Caller Identification
        See [Caller Identification in the WAMP Specification]
        (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.3.5)
        @{ */

    /** Requests that the identity (session ID) of this session be disclosed
        in the call invocation. */
    Rpc& withDiscloseMe(bool disclosed = true);
    /// @}

private:
    String procedure_;
    Error* error_ = nullptr;
    bool progressiveResultsEnabled_ = false;

public:
    String& procedure(internal::PassKey); // Internal use only
    Error* error(internal::PassKey); // Internal use only
};


//------------------------------------------------------------------------------
/** Contains the request ID and options passed to Session::cancel. */
//------------------------------------------------------------------------------
class CPPWAMP_API Cancellation : public Options<Cancellation>
{
public:
    /** Converting constructor. */
    Cancellation(RequestId reqId, CancelMode cancelMode = CancelMode::kill);

    RequestId requestId() const;

private:
    RequestId requestId_;
};


//------------------------------------------------------------------------------
/** Contains a remote procedure result yielded by a _callee_ or received by
    a _caller_. */
//------------------------------------------------------------------------------
class CPPWAMP_API Result : public Options<Result>, public Payload<Result>
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
    Result(RequestId reqId, Object&& details);

    RequestId reqId_ = -1;

public:
    // Internal use only
    Result(internal::PassKey, RequestId reqId, Object&& details);
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Result& result);


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

    /** Constructs an Outcome having Type::deferred. */
    static Outcome deferred();

    /** Default-constructs an outcome containing an empty Result object. */
    Outcome();

    /** Converting constructor taking a Result object. */
    Outcome(Result result);

    /** Converting constructor taking a braced initializer list of positional
        arguments to be stored in a Result. */
    Outcome(std::initializer_list<Variant> args);

    /** Converting constructor taking an Error object. */
    Outcome(Error error);

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
/** Contains payload arguments and other details related to a remote procedure
    call invocation.
    This class also provides the means for manually sending a `YIELD` or
    `ERROR` result back to the RPC caller. */
//------------------------------------------------------------------------------
class CPPWAMP_API Invocation : public Options<Invocation>,
                               public Payload<Invocation>
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
    AnyExecutor executor() const;

    /** Manually sends a `YIELD` result back to the callee. */
    void yield(Result result = Result()) const;

    /** Manually sends an `ERROR` result back to the callee. */
    void yield(Error error) const;

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
    Invocation(internal::PassKey, CalleePtr callee, RequestId id,
               AnyExecutor executor, Object&& details);

private:
    CalleePtr callee_;
    RequestId id_ = -1;
    AnyExecutor executor_ = nullptr;
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Invocation& inv);


//------------------------------------------------------------------------------
/** Contains details related to a remote procedure cancellation.
    This class also provides the means for manually sending a `YIELD` or
    `ERROR` result back to the RPC caller. */
//------------------------------------------------------------------------------
class CPPWAMP_API Interruption : public Options<Interruption>
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
    AnyExecutor executor() const;

    /** Manually sends a `YIELD` result back to the callee. */
    void yield(Result result = Result()) const;

    /** Manually sends an `ERROR` result back to the callee. */
    void yield(Error error) const;

public:
    // Internal use only
    using CalleePtr = std::weak_ptr<internal::Callee>;
    Interruption(internal::PassKey, CalleePtr callee, RequestId id,
                 AnyExecutor executor, Object&& details);

private:
    CalleePtr callee_;
    RequestId id_ = -1;
    AnyExecutor executor_ = nullptr;
};

CPPWAMP_API std::ostream& operator<<(std::ostream& out,
                                     const Interruption& cncltn);

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/sessiondata.ipp"
#endif


#endif // CPPWAMP_SESSIONDATA_HPP
