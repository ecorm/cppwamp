/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
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
#include "asiodefs.hpp"
#include "peerdata.hpp"
#include "options.hpp"
#include "payload.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    Contains declarations for data types exchanged via the client session
    interfaces. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** %Realm URI and other options passed to Session::join. */
//------------------------------------------------------------------------------
class Realm : public Options<Realm>
{
public:
    /** Converting constructor taking a realm URI. */
    Realm(String uri);

    /** Obtains the realm URI. */
    const String& uri() const;

    /** Sets the `HELLO.Details.authmethods` option. */
    Realm& withAuthMethods(std::vector<String> methods);

    /** Sets the `HELLO.Details.authid` option. */
    Realm& withAuthId(String authId);

private:
    String uri_;

public:
    String& uri(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Session information returned by Session::join. */
//------------------------------------------------------------------------------
class SessionInfo : public Options<SessionInfo>
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

    /** Obtains the agent string of the WAMP router, if available. */
    String agentString() const;

    /** Obtains the `WELCOME.Details.roles` dictionary for the router. */
    Object roles() const;

    /** Checks that the router supports the given set of roles. */
    bool supportsRoles(const RoleSet& roles) const;

    /** Checks that the router supports the given map of features. */
    bool supportsFeatures(const FeatureMap& features) const;

private:
    String realm_;
    SessionId sid_ = -1;

public:
    // Internal use only
    SessionInfo(internal::PassKey, String realm, SessionId id, Object details);
};

std::ostream& operator<<(std::ostream& out, const SessionInfo& info);

//------------------------------------------------------------------------------
/** %Topic URI and other options passed to Session::subscribe. */
//------------------------------------------------------------------------------
class Topic : public Options<Topic>
{
public:
    /** Converting constructor taking a topic URI. */
    Topic(String uri);

    /** Specifies that the _prefix-matching policy_ is to be used for this
        subscription. */
    Topic& usingPrefixMatch();

    /** Specifies that the _wildcard-matching policy_ is to be used for this
        subscription. */
    Topic& usingWildcardMatch();

    /** Obtains the topic URI. */
    const String& uri() const;

private:
    String uri_;

public:
    String& uri(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Contains the topic URI, options, and payload passed to Session::publish. */
//------------------------------------------------------------------------------
class Pub : public Options<Pub>, public Payload<Pub>
{
public:
    /** Converting constructor taking a topic URI. */
    Pub(String topic);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        won't receive the published event.
        @deprecated Use withExcludedSessions() instead. */
    Pub& withBlacklist(Array blacklist);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        won't receive the published event. */
    Pub& withExcludedSessions(Array sessionIds);

    /** Specifies a blacklist of authid strings. */
    Pub& withExcludedAuthIds(Array authIds);

    /** Specifies a blacklist of authrole strings. */
    Pub& withExcludedAuthRoles(Array authRoles);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        are allowed to receive the published event.
        @deprecated Use withEligibleSessions() instead. */
    Pub& withWhitelist(Array whitelist);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        are allowed to receive the published event. */
    Pub& withEligibleSessions(Array sessionIds);

    /** Specifies a whitelist of authid strings. */
    Pub& withEligibleAuthIds(Array authIds);

    /** Specifies a whitelist of authrole strings. */
    Pub& withEligibleAuthRoles(Array authRoles);

    /** Specifies if this session should be excluded from receiving the
        event. */
    Pub& withExcludeMe(bool excluded = true);

    /** Requests that the identity (session ID) of this session be disclosed
        in the event. */
    Pub& withDiscloseMe(bool disclosed = true);

private:
    String topic_;

public:
    String& topic(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Represents a published event */
//------------------------------------------------------------------------------
class Event : public Options<Event>, public Payload<Event>
{
public:
    /** Default constructor. */
    Event();

    /** Returns `false` if the Event has been initialized and is ready
        for use. */
    bool empty() const;

    /** Obtains the subscription ID associated with this event. */
    PublicationId subId() const;

    /** Obtains the publication ID associated with this event. */
    PublicationId pubId() const;

    /** Obtains the IO service used to execute user-provided handlers. */
    AsioService& iosvc() const;

    /** Obtains an optional publisher ID integer. */
    Variant publisher() const;

    /** Obtains an optional trust level integer. */
    Variant trustLevel() const;

    /** Obtains an optional string of the original topic URI used to make the
        publication. */
    Variant topic() const;

private:
    SubscriptionId subId_ = -1;
    PublicationId pubId_  = -1;
    AsioService* iosvc_   = nullptr;

public:
    // Internal use only
    Event(internal::PassKey, SubscriptionId subId, PublicationId pubId,
          AsioService* iosvc, Object&& details);
};

std::ostream& operator<<(std::ostream& out, const Event& event);

//------------------------------------------------------------------------------
/** %Procedure URI and other options passed to Session::enroll. */
//------------------------------------------------------------------------------
class Procedure : public Options<Procedure>
{
public:
    /** Converting constructor taking a procedure URI. */
    Procedure(String uri);

    /** Specifies that the _prefix-matching policy_ is to be used for this
        registration. */
    Procedure& usingPrefixMatch();

    /** Specifies that the _wildcard-matching policy_ is to be used for this
        subscription. */
    Procedure& usingWildcardMatch();

    /** Requests that the identity (session ID) of this session be disclosed
        in the remote procedure call. */
    Procedure& withDiscloseCaller(bool disclosed = true);

    /** Obtains the procedure URI. */
    const String& uri() const;

private:
    String uri_;

public:
    String& uri(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Contains the procedure URI, options, and payload passed to Session::call. */
//------------------------------------------------------------------------------
class Rpc : public Options<Rpc>, public Payload<Rpc>
{
public:
    /** Converting constructor taking a procedure URI. */
    Rpc(String procedure);

    /** Specifies the Error object in which to store call errors returned
        by the callee. */
    Rpc& captureError(Error& error);

    /** Requests that the dealer cancels the call after the specified
        timeout duration. */
    Rpc& withDealerTimeout(Int milliseconds);

    /** Specifies the list of (potential) _Callee_ session IDs that a call
        won't be forwarded to.
        @deprecated This feature has been removed from the WAMP spec. */
    Rpc& withBlacklist(Array blacklist);

    /** Specifies the list of (potential) _Callee_ session IDs that are
        issued the call.
        @deprecated This feature has been removed from the WAMP spec. */
    Rpc& withWhitelist(Array whitelist);

    /** Specifies if this session should be excluded from receiving the
        call invocation.
        @deprecated This feature has been removed from the WAMP spec. */
    Rpc& withExcludeMe(bool excluded = true);

    /** Requests that the identity (session ID) of this session be disclosed
        in the call invocation. */
    Rpc& withDiscloseMe(bool disclosed = true);

private:
    String procedure_;
    Error* error_ = nullptr;

public:
    String& procedure(internal::PassKey); // Internal use only
    Error* error(internal::PassKey); // Internal use only
};


//------------------------------------------------------------------------------
/** Contains a remote procedure result yielded by a _callee_ or received by
    a _caller_. */
//------------------------------------------------------------------------------
class Result : public Options<Result>, public Payload<Result>
{
public:
    /** Default constructor. */
    Result();

    /** Converting constructor taking a braced initializer list of
        positional arguments. */
    Result(std::initializer_list<Variant> list);

    /** Lets the callee specify if the yielded result is progressive. */
    Result& withProgress(bool progressive = true);

    /** Obtains the request ID associated with the call. */
    RequestId requestId() const;

private:
    Result(RequestId reqId, Object&& details);

    RequestId reqId_ = -1;

public:
    // Internal use only
    Result(internal::PassKey, RequestId reqId, Object&& details);
};

std::ostream& operator<<(std::ostream& out, const Result& result);


namespace internal { class Callee; } // Forward declaration


//------------------------------------------------------------------------------
/** Contains the outcome of an RPC invocation.
    @see @ref RpcOutcomes */
//------------------------------------------------------------------------------
class Outcome
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
    explicit Outcome(std::nullptr_t);
    void copyFrom(const Outcome& other);
    void moveFrom(Outcome&& other);
    void destruct();

    Type type_;

    union Value
    {
        Value() {}
        ~Value() {}

        Result result;
        Error error;
    } value_;
};


//------------------------------------------------------------------------------
/** Contains payload arguments and other details related to a remote procedure
    call invocation.
    This class also provides the means for manually sending a `YIELD` or
    `ERROR` result back to the RPC caller. */
//------------------------------------------------------------------------------
class Invocation : public Options<Invocation>, public Payload<Invocation>
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

    /** Obtains the IO service used to execute user-provided handlers. */
    AsioService& iosvc() const;

    /** Checks if the caller requested progressive results. */
    bool isProgressive() const;

    /** Returns an optional session ID integer of the caller. */
    Variant caller() const;

    /** Returns an optional trust level integer. */
    Variant trustLevel() const;

    /** Returns an optional string of the original procedure URI used to make
        this call. */
    Variant procedure() const;

    /** Manually sends a `YIELD` result back to the callee. */
    void yield(Result result = Result()) const;

    /** Manually sends an `ERROR` result back to the callee. */
    void yield(Error error) const;

public:
    // Internal use only
    using CalleePtr = std::weak_ptr<internal::Callee>;
    Invocation(internal::PassKey, CalleePtr callee, RequestId id,
               AsioService* iosvc, Object&& details);

private:
    CalleePtr callee_;
    RequestId id_ = -1;
    AsioService* iosvc_ = nullptr;
};

std::ostream& operator<<(std::ostream& out, const Invocation& inv);


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/sessiondata.ipp"
#endif


#endif // CPPWAMP_SESSIONDATA_HPP
