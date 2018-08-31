/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSION_HPP
#define CPPWAMP_SESSION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the asynchronous session API used by a _client_ peer in WAMP
    applications. */
//------------------------------------------------------------------------------

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "asiodefs.hpp"
#include "asyncresult.hpp"
#include "peerdata.hpp"
#include "connector.hpp"
#include "error.hpp"
#include "registration.hpp"
#include "sessiondata.hpp"
#include "subscription.hpp"
#include "wampdefs.hpp"
#include "internal/asynctask.hpp"
#include "internal/clientinterface.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Asynchronous session API used by a _client_ peer in WAMP applications.

    @par Roles
    This API supports all of the WAMP _client_ roles:
    - _Callee_
    - _Caller_
    - _Publisher_
    - _Subscriber_

    @par Asynchronous Operations
    Most of Session's member functions are asynchronous and thus require a
    handler function that is invoked when the operation is completed. For
    asynchronous operations that can fail, a handler taking an AsyncResult as
    a parameter is required. AsyncResult makes it impossible for handlers to
    ignore error conditions when accessing the result of an asynchronous
    operation.

    @note In the detailed documentation of asynchronous operations, items
          listed under **Returns** refer to results that are returned via
          AsyncResult.

    The `boost::asio::io_service` passed via `create()` is used when executing
    handler functions passed-in by the user. This can be the same, or different
    than the `io_service` passed to the `Connector` creation functions.

    @par Aborting Asynchronous Operations
    All pending asynchronous operations can be _aborted_ by dropping the client
    connection via Session::disconnect. Pending post-join operations can be also
    be aborted via Session::leave. There is currently no way to abort a single
    operation without dropping the connection or leaving the realm.

    @par Terminating Asynchronous Operations
    All pending asynchronous operations can be _terminated_ by dropping the
    client connection via Session::reset or the Session destructor. By design,
    the handlers for pending operations will not be invoked if they
    were terminated in this way. This is useful if a client application needs
    to shutdown abruptly and cannot enforce the lifetime of objects accessed
    within the asynchronous operation handlers.

    @par Coroutine API
    To make it easier to chain successive asynchronous operations, a
    coroutine-based API is provided via CoroSession.

    @see AsyncHandler, AsyncResult, CoroSession, Registration, Subscription. */
//------------------------------------------------------------------------------
class Session : public std::enable_shared_from_this<Session>
{
public:
    /** Shared pointer to a Session. */
    using Ptr = std::shared_ptr<Session>;

    /** Enumerates the possible states that a Session can be in. */
    using State = SessionState;

    /** Handler type used for processing log events. */
    using LogHandler = std::function<void (std::string)>;

    /** Function type handling authtication challenges. */
    using ChallengeHandler = std::function<void (Challenge)>;

    /** Function type for handling pub/sub events. */
    using EventSlot = std::function<void (Event)>;

    /** Function type for handling remote procedure calls. */
    using CallSlot = std::function<Outcome (Invocation)>;

    /** Function type for handling RPC interruptions. */
    using InterruptSlot = std::function<Outcome (Interruption)>;

    /** Creates a new Session instance. */
    static Ptr create(AsioService& userIosvc, const Connector::Ptr& connector);

    /** Creates a new Session instance. */
    static Ptr create(AsioService& userIosvc, const ConnectorList& connectors);

    /** Obtains a dictionary of roles and features supported on the client
        side. */
    static const Object& roles();

    /// @name Non-copyable
    /// @{
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    /// @}

    /** Destructor. */
    virtual ~Session();

    /// @name Observers
    /// @{

    /** Obtains the IO service used to execute user-provided handlers. */
    AsioService& userIosvc() const;

    /** Returns the current state of the session. */
    SessionState state() const;
    /// @}

    /// @name Modifiers
    /// @{
    /** Sets the log handler for warnings. */
    void setWarningHandler(LogHandler handler);

    /** Sets the log handler for debug traces. */
    void setTraceHandler(LogHandler handler);

    /** Sets the handler for authentication challenges. */
    void setChallengeHandler(ChallengeHandler handler);
    /// @}

    /// @name Session Management
    /// @{
    /** Asynchronously attempts to connect to a router. */
    void connect(AsyncHandler<size_t> handler);

    /** Asynchronously attempts to join the given WAMP realm. */
    void join(Realm realm, AsyncHandler<SessionInfo> handler);

    /** Sends an `AUTHENTICATE` in response to a `CHALLENGE`. */
    void authenticate(Authentication auth);

    /** Asynchronously leaves the WAMP session. */
    void leave(Reason reason, AsyncHandler<Reason> handler);

    /** Disconnects the transport between the client and router. */
    void disconnect();

    /** Terminates the transport connection between the client and router. */
    void reset();
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    void subscribe(Topic topic, EventSlot slot,
                   AsyncHandler<Subscription> handler);

    /** Unsubscribes a subscription to a topic. */
    void unsubscribe(const Subscription& sub);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement, if necessary. */
    void unsubscribe(const Subscription& sub, AsyncHandler<bool> handler);

    /** Publishes an event. */
    void publish(Pub pub);

    /** Publishes an event and waits for an acknowledgement from the router. */
    void publish(Pub pub, AsyncHandler<PublicationId> handler);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    void enroll(Procedure procedure, CallSlot callSlot,
                AsyncHandler<Registration> handler);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    void enroll(Procedure procedure, CallSlot slot, InterruptSlot interruptSlot,
                AsyncHandler<Registration> handler);

    /** Unregisters a remote procedure call. */
    void unregister(const Registration& reg);

    /** Unregisters a remote procedure call and waits for router
        acknowledgement. */
    void unregister(const Registration& reg, AsyncHandler<bool> handler);

    /** Calls a remote procedure. */
    RequestId call(Rpc procedure, AsyncHandler<Result> handler);

    /** Cancels a remote procedure. */
    void cancel(Cancellation cancellation);
    /// @}

protected:
    explicit Session(AsioService& userIosvc, const ConnectorList& connectors);

    void doConnect(size_t index, AsyncTask<size_t> handler);

    std::shared_ptr<internal::ClientInterface> impl();

private:
    AsioService& userIosvc_;
    ConnectorList connectors_;
    Connector::Ptr currentConnector_;
    AsyncTask<std::string> warningHandler_;
    AsyncTask<std::string> traceHandler_;
    AsyncTask<Challenge> challengeHandler_;
    SessionState state_ = State::disconnected;
    bool isTerminating_ = false;
    std::shared_ptr<internal::ClientInterface> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/session.ipp"
#endif

#endif // CPPWAMP_SESSION_HPP
