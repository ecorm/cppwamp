/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSIONDATA_HPP
#define CPPWAMP_SESSIONDATA_HPP

#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include "dialoguedata.hpp"
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
    /** Constructor taking a realm URI. */
    explicit Realm(String uri);

    /** Obtains the realm URI. */
    const String& uri() const;

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
    using RoleSet    = std::set<String>;

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
    /** Constructor taking a topic URI. */
    explicit Topic(String uri);

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
    /** Constructor taking a topic URI. */
    explicit Pub(String topic);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        won't receive the published event. */
    Pub& withBlacklist(Array blacklist);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        are allowed to receive the published event. */
    Pub& withWhitelist(Array whitelist);

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

    /** Obtains the subscription ID associated with this event. */
    PublicationId subId() const;

    /** Obtains the publication ID associated with this event. */
    PublicationId pubId() const;

    /** Obtains an optional publisher ID integer. */
    Variant publisher() const;

    /** Obtains an optional trust level integer. */
    Variant trustLevel() const;

    /** Obtains an optional string of the original topic URI used to make the
        publication. */
    Variant topic() const;

private:
    SubscriptionId subId_ = -1;
    PublicationId pubId_ = -1;

public:
    // Internal use only
    Event(internal::PassKey, SubscriptionId subId, PublicationId pubId,
          Object&& details);
};

std::ostream& operator<<(std::ostream& out, const Event& event);

//------------------------------------------------------------------------------
/** %Procedure URI and other options passed to Session::enroll. */
//------------------------------------------------------------------------------
class Procedure : public Options<Procedure>
{
public:
    /** Constructor taking a procedure URI. */
    explicit Procedure(String uri);

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
    /** Constructor taking a procedure URI. */
    explicit Rpc(String procedure);

    /** Requests that the dealer cancels the call after the specified
        timeout duration. */
    Rpc& withDealerTimeout(Int milliseconds);

    /** Specifies the list of (potential) _Callee_ session IDs that a call
        won't be forwarded to. */
    Rpc& withBlacklist(Array blacklist);

    /** Specifies the list of (potential) _Callee_ session IDs that are
        issued the call. */
    Rpc& withWhitelist(Array whitelist);

    /** Specifies if this session should be excluded from receiving the
        call invocation. */
    Rpc& withExcludeMe(bool excluded = true);

    /** Requests that the identity (session ID) of this session be disclosed
        in the call invocation. */
    Rpc& withDiscloseMe(bool disclosed = true);

private:
    String procedure_;

public:
    String& procedure(internal::PassKey); // Internal use only
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
/** Provides the means for returning a `YIELD` or `ERROR` result back to
    the RPC caller. */
//------------------------------------------------------------------------------
class Invocation : public Options<Invocation>, public Payload<Invocation>
{
public:
    /** Default constructor */
    Invocation();

    /** Determines if the Session object that dispatched this
        invocation still exists or has expired. */
    bool calleeHasExpired() const;

    /** Returns the request ID associated with this RPC invocation. */
    RequestId requestId() const;

    /** Checks if the caller requested progressive results. */
    bool isProgressive() const;

    /** Returns an optional session ID integer of the caller. */
    Variant caller() const;

    /** Returns an optional trust level integer. */
    Variant trustLevel() const;

    /** Returns an optional string of the original procedure URI used to make
        this call. */
    Variant procedure() const;

    /** Sends a `YIELD` result back to the callee. */
    void yield(Result result = Result());

    /** Sends an `ERROR` result back to the callee. */
    void yield(Error error);

public:
    // Internal use only
    using CalleePtr = std::weak_ptr<internal::Callee>;
    Invocation(internal::PassKey, CalleePtr callee, RequestId id,
               Object&& details);

private:
    CalleePtr callee_;
    RequestId id_ = -1;
};

std::ostream& operator<<(std::ostream& out, const Invocation& inv);


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/sessiondata.ipp"
#endif


#endif // CPPWAMP_SESSIONDATA_HPP
