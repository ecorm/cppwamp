/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSION_HPP
#define CPPWAMP_SESSION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the session API used by a _client_ peer in
           WAMP applications. */
//------------------------------------------------------------------------------

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <boost/asio/async_result.hpp>
#include "any.hpp"
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "calleestreaming.hpp"
#include "callerstreaming.hpp"
#include "clientinfo.hpp"
#include "config.hpp"
#include "connector.hpp"
#include "exceptions.hpp"
#include "errorcodes.hpp"
#include "erroror.hpp"
#include "pubsubinfo.hpp"
#include "registration.hpp"
#include "rpcinfo.hpp"
#include "subscription.hpp"
#include "traits.hpp"
#include "timeout.hpp"
#include "wampdefs.hpp"

// TODO: Default completion token
// TODO: User-defined agent string

namespace wamp
{

// Forward declarations
namespace internal
{
class Client;
class Peer;
}

//------------------------------------------------------------------------------
/** %Session API used by a _client_ peer in WAMP applications.

    @par Roles
    This API supports all of the WAMP _client_ roles:
    - _Callee_
    - _Caller_
    - _Publisher_
    - _Subscriber_

    @par Asynchronous Operations
    Most of Session's member functions are asynchronous and thus require a
    _completion token_ that is invoked when the operation is completed. All
    asynchronous operations emit an ErrorOr as the result. ErrorOr makes
    it difficult for handlers to ignore error conditions when accessing the
    result of an asynchronous operation.

    @note In the detailed documentation of asynchronous operations, items
          listed under **Returns** refer to results that are emitted via
          ErrorOr.

    @par Fallback Executor
    A *fallback executor* may optionally be passed to Session for use in
    executing user-provided handlers. If there is no executor bound to the
    handler, Session will use Session::fallbackExecutor() instead.

    @par Aborting Asynchronous Operations
    All pending asynchronous operations can be _aborted_ by dropping the client
    connection via Session::disconnect, or by destroying the Session object.
    Pending post-join operations can be also be aborted via Session::leave.
    Except for RPCs, there is currently no way to abort a single operation
    without dropping the connection or leaving the realm.

    @par Terminating Asynchronous Operations
    All pending asynchronous operations can be _terminated_ via Session::reset.
    When terminating, the handlers for pending operations will not be invoked
    (this will result in hung coroutine operations). This is useful if a client
    application needs to shutdown abruptly and cannot enforce the lifetime of
    objects accessed within the asynchronous operation handlers.

    @par Thread-safety
    All operations are made sequential via the Session's execution
    [strand](https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/strands.html).
    This makes all of Session's methods thread-safe.

    @par Notable Error Codes
    - MiscErrc::invalidState if the session was not in the appropriate state
      for a given operation
    - MiscErrc::abandoned if an operation was aborted by the user closing
      the session
    - WampErrc::invalidUri if the router rejected a malformed URI
    - WampErrc::sessionKilled if an operation was aborted due the session
      being killed by the peer
    - WampErrc::authorizationDenied if the router rejected an unauthorized operation
    - WampErrc::optionNotAllowed if the router does does support an option
    - WampErrc::featureNotSupported if the router rejected an attempt to use
      an unsupported WAMP feature
    - WampErrc::payloadSizeExceeded if a resulting WAMP message exceeds
      the transport's limits

    @see ErrorOr, Registration, Subscription. */
//------------------------------------------------------------------------------
class CPPWAMP_API Session
{
public:
    /** Executor type used for I/O operations. */
    using Executor = AnyIoExecutor;

    /** Fallback executor type for user-provided handlers. */
    using FallbackExecutor = AnyCompletionExecutor;

    /** Enumerates the possible states that a Session can be in. */
    using State = SessionState;

    /** Obtains the type returned by [boost::asio::async_initiate]
        (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/async_initiate.html)
        with given the completion token type `C` and signature `void(T)`.

        Token Type                   | Deduced Return Type
        ---------------------------- | -------------------
        Callback function            | `void`
        `wamp::YieldContext`         | `ErrorOr<Value>`
        `boost::asio::use_awaitable` | An awaitable yielding `ErrorOr<Value>`
        `boost::asio::use_future`    | `std::future<ErrorOr<Value>>` */
    template <typename T, typename C>
    using Deduced =
        typename boost::asio::async_result<Decay<C>, void(T)>::return_type;

    /// @name Construction
    /// @{
    /** Constructor taking an executor. */
    explicit Session(Executor exec);

    /** Constructor taking an executor for I/O operations
        and another for user-provided handlers. */
    Session(Executor exec, FallbackExecutor fallbackExec);

    /** Constructor taking an execution context. */
    template <typename E, CPPWAMP_NEEDS(isExecutionContext<E>()) = 0>
    explicit Session(E& context) : Session(context.get_executor()) {}

    /** Constructor taking an I/O execution context and another as fallback
        for user-provided handlers. */
    template <typename E1, typename E2,
              CPPWAMP_NEEDS(isExecutionContext<E1>() &&
                            isExecutionContext<E2>()) = 0>
    explicit Session(E1& executionContext, E2& fallbackExecutionContext)
        : Session(executionContext.get_executor(),
                  fallbackExecutionContext.get_executor())
    {}

    /** Destructor. */
    virtual ~Session(); // NOLINT(bugprone-exception-escape)
    /// @}

    /// @name Move-only
    /// @{
    Session(const Session&) = delete;
    Session(Session&&) = default;
    Session& operator=(const Session&) = delete;
    Session& operator=(Session&&) = default;
    /// @}

    /// @name Observers
    /// @{

    /** Obtains the execution context in which I/O operations are serialized. */
    const IoStrand& strand() const;

    /** Obtains the executor that was passed during construction. */
    const Executor& executor() const;

    /** Obtains the fallback executor used for user-provided handlers. */
    const FallbackExecutor& fallbackExecutor() const;

    /** Returns the current state of the session. */
    SessionState state() const;
    /// @}

    /// @name Modifiers
    /// @{
    /** Sets the handler that is dispatched for session incidents. */
    template <typename S>
    void observeIncidents(S&& incidentSlot);

    /** Enables message tracing incidents. */
    void enableTracing(bool enabled = true);

    /** Sets the default timeout duration to use for an operation without
        a specified timeout. */
    void setFallbackTimeout(Timeout timeout);
    /// @}

    /// @name Session Management
    /// @{
    /** Asynchronously attempts to connect to a router. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(ConnectionWish wish, C&& completion);

    /** Asynchronously attempts to connect to a router. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(ConnectionWishList wishes, C&& completion);

    /** Asynchronously attempts to join the given WAMP realm. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Welcome>, C>
    join(Petition realm, C&& completion);

    /** Asynchronously attempts to join the given WAMP realm, using the given
        authentication challenge handler. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Welcome>, C>
    join(Petition realm, S&& challengeSlot, C&& completion);

    /** Asynchronously leaves the WAMP session. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Reason>, C>
    leave(C&& completion);

    /** Asynchronously leaves the WAMP session with the given reason. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Reason>, C>
    leave(Reason reason, C&& completion);

    /** Asynchronously leaves the WAMP session with the given reason and
        timeout duration. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Reason>, C>
    leave(Reason reason, Timeout timeout, C&& completion);

    /** Gracefully closes the transport between the client and router. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    disconnect(C&& completion);

    /** Gracefully closes the transport using the given timeout duration. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    disconnect(Timeout timeout, C&& completion);

    /** Abruptly disconnects the transport between the client and router. */
    void disconnect();

    /** Terminates the transport connection between the client and router. */
    void terminate();
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Subscription>, C>
    subscribe(Topic topic, S&& eventSlot, C&& completion);

    /** Unsubscribes a subscription to a topic. */
    void unsubscribe(Subscription sub);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement, if necessary. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unsubscribe(Subscription sub, C&& completion);

    /** Unsubscribes a subscription to a topic and waits up until until the
        given timeout period for router acknowledgement, if necessary. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unsubscribe(Subscription sub, Timeout timeout, C&& completion);

    /** Publishes an event. */
    void publish(Pub pub);

    /** Publishes an event and waits for an acknowledgement from the router. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<PublicationId>, C>
    publish(Pub pub, C&& completion);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Procedure procedure, S&& callSlot, C&& completion);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    template <typename S, typename I, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Procedure procedure, S&& callSlot, I&& interruptSlot,
           C&& completion);

    /** Unregisters a remote procedure call or stream. */
    void unregister(Registration reg);

    /** Unregisters a remote procedure call and waits for router
        acknowledgement. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unregister(Registration reg, C&& completion);

    /** Unregisters a remote procedure call and waits up until the given
        timeout period for router acknowledgement. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unregister(Registration reg, Timeout timeout, C&& completion);

    /** Calls a remote procedure. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(Rpc rpc, C&& completion);
    /// @}

    /// @name Streaming
    /// @{
    /** Registers a streaming endpoint. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Stream stream, S&& streamSlot, C&& completion);

    /** Sends a request to open a stream and waits for an initial response. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<CallerChannel>, C>
    requestStream(StreamRequest req, S&& chunkSlot, C&& completion);

    /** Sends a request to open a stream and waits for an initial response. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<CallerChannel>, C>
    requestStream(StreamRequest req, C&& completion);

    /** Opens a streaming channel without negotiation. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<CallerChannel>, C>
    openStream(StreamRequest req, S&& chunkSlot, C&& completion);

    /** Opens a streaming channel without negotiation. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<CallerChannel>, C>
    openStream(StreamRequest req, C&& completion);
    /// @}

protected:
    Session(std::shared_ptr<internal::Peer> peer, Executor exec);

    Session(std::shared_ptr<internal::Peer> peer, Executor exec,
            FallbackExecutor fallbackExec);

    void directConnect(any link);

private:
    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<T>)>;

    using IncidentSlot  = AnyReusableHandler<void (Incident)>;
    using ChallengeSlot = AnyReusableHandler<void (Challenge)>;
    using EventSlot     = AnyReusableHandler<void (Event)>;
    using CallSlot      = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;
    using StreamSlot    = AnyReusableHandler<void (CalleeChannel)>;
    using CallerChunkSlot =
        AnyReusableHandler<void (CallerChannel, ErrorOr<CallerInputChunk>)>;

    // These initiator function objects are needed due to C++11 lacking
    // generic lambdas.
    struct ConnectOp;
    struct JoinOp;
    struct LeaveOp;
    struct GracefulDisconnectOp;
    struct SubscribeOp;
    struct UnsubscribeOp;
    struct PublishOp;
    struct EnrollOp;
    struct EnrollIntrOp;
    struct UnregisterOp;
    struct CallOp;
    struct OngoingCallOp;
    struct EnrollStreamOp;
    struct RequestStreamOp;
    struct OpenStreamOp;

    template <typename F>
    typename internal::BindFallbackExecutorResult<F>::Type
    bindFallbackExecutor(F&& handler) const;

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    initiate(C&& token, As&&... args);

    bool canUnsubscribe(const Subscription& sub) const;
    bool canUnregister(const Registration& reg) const;
    void setIncidentHandler(IncidentSlot&& s);
    void doConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f);
    void doJoin(Petition&& p, ChallengeSlot&& s,
                CompletionHandler<Welcome>&& f);
    void doLeave(Reason&& reason, Timeout t, CompletionHandler<Reason>&& f);
    void doDisconnect(Timeout t, CompletionHandler<bool>&& f);
    void doSubscribe(Topic&& t, EventSlot&& s,
                     CompletionHandler<Subscription>&& f);
    void doUnsubscribe(const Subscription& s, Timeout t,
                       CompletionHandler<bool>&& f);
    void doPublish(Pub&& p, CompletionHandler<PublicationId>&& f);
    void doEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                  CompletionHandler<Registration>&& f);
    void doUnregister(const Registration& r, Timeout t,
                      CompletionHandler<bool>&& f);
    void doCall(Rpc&& r, CompletionHandler<Result>&& f);
    void doEnroll(Stream&& s, StreamSlot&& ss,
                  CompletionHandler<Registration>&& f);
    void doRequestStream(StreamRequest&& r, CallerChunkSlot&& c,
                         CompletionHandler<CallerChannel>&& f);
    void doOpenStream(StreamRequest&& r, CallerChunkSlot&& s,
                      CompletionHandler<CallerChannel>&& f);

    FallbackExecutor fallbackExecutor_;
    std::shared_ptr<internal::Client> impl_;
};


//******************************************************************************
// Session template function implementations
//******************************************************************************

// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
//------------------------------------------------------------------------------
struct Session::JoinOp
{
    using ResultValue = Welcome;
    Session* self;
    Petition p;
    ChallengeSlot s;

    template <typename F> void operator()(F&& f)
    {
        self->doJoin(std::move(p), std::move(s),
                     self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::LeaveOp
{
    using ResultValue = Reason;
    Session* self;
    Reason r;
    Timeout t;

    template <typename F> void operator()(F&& f)
    {
        self->doLeave(std::move(r), t,
                      self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::GracefulDisconnectOp
{
    using ResultValue = bool;
    Session* self;
    Timeout t;

    template <typename F> void operator()(F&& f)
    {
        self->doDisconnect(t, self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::SubscribeOp
{
    using ResultValue = Subscription;
    Session* self;
    Topic t;
    EventSlot s;

    template <typename F> void operator()(F&& f)
    {
        self->doSubscribe(std::move(t), std::move(s),
                          self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::UnsubscribeOp
{
    using ResultValue = bool;
    Session* self;
    Subscription s;
    Timeout t;

    template <typename F> void operator()(F&& f)
    {
        self->doUnsubscribe(s, t,
                            self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::PublishOp
{
    using ResultValue = PublicationId;
    Session* self;
    Pub p;

    template <typename F> void operator()(F&& f)
    {
        self->doPublish(std::move(p),
                        self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::EnrollOp
{
    using ResultValue = Registration;
    Session* self;
    Procedure p;
    CallSlot s;
    InterruptSlot i;

    template <typename F> void operator()(F&& f)
    {
        self->doEnroll(std::move(p), std::move(s), std::move(i),
                       self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::UnregisterOp
{
    using ResultValue = bool;
    Session* self;
    Registration r;
    Timeout t;

    template <typename F> void operator()(F&& f)
    {
        self->doUnregister(r, t,
                           self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::CallOp
{
    using ResultValue = Result;
    Session* self;
    Rpc r;

    template <typename F> void operator()(F&& f)
    {
        self->doCall(std::move(r),
                     self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::EnrollStreamOp
{
    using ResultValue = Registration;
    Session* self;
    Stream s;
    StreamSlot ss;

    template <typename F> void operator()(F&& f)
    {
        self->doEnroll(std::move(s), std::move(ss),
                       self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::RequestStreamOp
{
    using ResultValue = CallerChannel;
    Session* self;
    StreamRequest r;
    CallerChunkSlot c;

    template <typename F> void operator()(F&& f)
    {
        self->doRequestStream(std::move(r), std::move(c),
                              self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
struct Session::OpenStreamOp
{
    using ResultValue = CallerChannel;
    Session* self;
    StreamRequest r;
    CallerChunkSlot c;

    template <typename F> void operator()(F&& f)
    {
        self->doOpenStream(std::move(r), std::move(c),
                           self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

// NOLINTEND(cppcoreguidelines-pro-type-member-init)


//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature `void (Incident)`
    @details
    Copies of the handler are made when they are dispatched. If the handler
    needs to be stateful, or is non-copyable, then pass a stateless copyable
    proxy instead.
    @note No incident events are fired when the session object is terminating.
    @see Session::enableTracing */
//------------------------------------------------------------------------------
template <typename S>
void Session::observeIncidents(S&& incidentSlot)
{
    setIncidentHandler(bindFallbackExecutor(std::forward<S>(incidentSlot)));
}

//------------------------------------------------------------------------------
struct Session::ConnectOp
{
    using ResultValue = size_t;
    Session* self;
    ConnectionWishList w;

    template <typename F> void operator()(F&& f)
    {
        self->doConnect(std::move(w),
                        self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
/** @tparam C A callable handler of type `void(ErrorOr<size_t>)`, or a
              compatible Boost.Asio completion token.
    @details
    The session will attempt to connect using the transport/codec combinations
    specified in the given ConnectionWishList, in the same order.
    @return The index of the ConnectionWish used to establish the connetion
            (always zero for this overload).
    @post `this->state() == SessionState::connecting` if successful
    @par Notable Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - TransportErrc::exhausted if more than one transport was
          specified and they all failed to connect. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<std::size_t>, C>
#else
Session::template Deduced<ErrorOr<std::size_t>, C>
#endif
Session::connect(
    ConnectionWish wish, /**< Transport/codec combination to use for
                              attempting connection. */
    C&& completion       /**< Completion handler or token. */
    )
{
    return connect(ConnectionWishList{std::move(wish)},
                   std::forward<C>(completion));
}

//------------------------------------------------------------------------------
/** @tparam C A callable handler of type `void(ErrorOr<size_t>)`, or a
              compatible Boost.Asio completion token
    @details
    The session will attempt to connect using the transport/codec combinations
    specified in the given ConnectionWishList, in the same order.
    @return The index of the ConnectionWish used to establish the connetion.
    @pre `wishes.empty() == false`
    @post `this->state() == SessionState::connecting` if successful
    @throws error::Logic if the given wish list is empty
    @par Notable Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - TransportErrc::exhausted if more than one transport was
          specified and they all failed to connect. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<std::size_t>, C>
#else
Session::template Deduced<ErrorOr<std::size_t>, C>
#endif
Session::connect(
    ConnectionWishList wishes, /**< List of transport addresses/options/codecs
                                    to use for attempting connection. */
    C&& completion             /**< Completion handler or token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!wishes.empty(),
                        "Session::connect ConnectionWishList cannot be empty");
    return initiate<ConnectOp>(std::forward<C>(completion), std::move(wishes));
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<Welcome>)`, or a
              compatible Boost.Asio completion token
    @return A Welcome object with details on the newly established session.
    @post `this->state() == SessionState::establishing` if successful
    @par Notable Error Codes
        - WampErrc::noSuchRealm if the realm does not exist.
        - WampErrc::noSuchRole if one of the client roles is not supported on
          the router.
        - WampErrc::authenticationDenied if the router rejected the request
          to join. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Welcome>, C>
#else
Session::template Deduced<ErrorOr<Welcome>, C>
#endif
Session::join(
    Petition realm, ///< Details on the realm to join.
    C&& completion  ///< Completion handler or token.
    )
{
    return initiate<JoinOp>(std::forward<C>(completion), std::move(realm),
                            nullptr);
}

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature `void (Challenge)`
    @copydetails Session::join(Petition, C&&)
    @note A copy of the challenge handler is made when it is dispatched. If the
    handler needs to be stateful, or is non-copyable, then pass a stateless
    copyable proxy instead. */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Welcome>, C>
#else
Session::template Deduced<ErrorOr<Welcome>, C>
#endif
Session::join(
    Petition realm,    /**< Details on the realm to join. */
    S&& challengeSlot, /**< Handles authentication challenges. */
    C&& completion     /**< Completion handler or token. */
    )
{
    return initiate<JoinOp>(
        bindFallbackExecutor(std::forward<C>(completion)), std::move(realm),
        std::forward<S>(challengeSlot));
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<Reason>)`, or a
              compatible Boost.Asio completion token.
    @details The "wamp.close.close_realm" reason is sent as part of the
             outgoing `GOODBYE` message.
    @return The _Reason_ URI and details from the `GOODBYE` response returned
            by the router.
    @post `this->state() == SessionState::shuttingDown` if successful */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<Reason>, C>
#endif
Session::leave(
    C&& completion ///< Completion handler or token.
    )
{
    return leave(Reason{WampErrc::closeRealm}, std::forward<C>(completion));
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<Reason>)`, or a
              compatible Boost.Asio completion token.
    @return The _Reason_ URI and details from the `GOODBYE` response returned
            by the router.
    @post `this->state() == SessionState::shuttingDown` if successful */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<Reason>, C>
#endif
Session::leave(
    Reason reason, ///< %Reason URI and other options.
    C&& completion ///< Completion handler or token.
    )
{
    return initiate<LeaveOp>(std::forward<C>(completion), std::move(reason),
                             unspecifiedTimeout);
}

//------------------------------------------------------------------------------
/** @copydetails Session::leave(Reason, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<Reason>, C>
#endif
Session::leave(
    Reason reason,   ///< %Reason URI and other options.
    Timeout timeout, ///< Timeout duration.
    C&& completion   ///< Completion handler or token.
    )
{
    return initiate<LeaveOp>(std::forward<C>(completion), std::move(reason),
                             timeout);
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<bool>)`, or a
              compatible Boost.Asio completion token.
    @return `true` if the transport closed successfully, `false` if the
            transport was not connected, or an error code if the close handshake
            operation failed.
    @post `this->state() == SessionState::disconnecting` if transitioning
           from a state where the transport is connected, otherwise
          `this->state() == SessionState::disconnected`. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::disconnect(
    C&& completion ///< Completion handler or token.
    )
{
    return initiate<GracefulDisconnectOp>(std::forward<C>(completion),
                                          unspecifiedTimeout);
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<bool>)`, or a
              compatible Boost.Asio completion token.
    @copydetails Session::disconnect(C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::disconnect(
    Timeout timeout, ///< Timeout duration.
    C&& completion   ///< Completion handler or token.
    )
{
    return initiate<GracefulDisconnectOp>(std::forward<C>(completion),
                                          timeout);
}

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature `void (Event)`
    @tparam C Callable handler with signature `void (ErrorOr<Subscription>)`,
              or a compatible Boost.Asio completion token
    @pre topic.matchPolicy() != MatchPolicy::unknown
    @throws error::Logic if the given topic contains an unknown match policy.
    @return A Subscription object, therafter used to manage the subscription's
            lifetime.
    @see @ref Subscriptions */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Subscription>, C>
#else
Session::template Deduced<ErrorOr<Subscription>, C>
#endif
Session::subscribe(
    Topic topic,   ///< The topic to subscribe to.
    S&& eventSlot, ///< Event handler.
    C&& completion ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(topic.matchPolicy() != MatchPolicy::unknown,
                        "Unsupported match policy for subscribe operation");
    return initiate<SubscribeOp>(
        std::forward<C>(completion), std::move(topic),
        bindFallbackExecutor(std::forward<S>(eventSlot)));
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<bool>)`,
              or a compatible Boost.Asio completion token.
    @details
    If there are other local subscriptions on this session remaining for the
    same topic, then the session does not send an `UNSUBSCRIBE` message to
    the router and `true` will be passed to the completion handler.
    If the subscription is no longer applicable, then this operation will
    effectively do nothing and a `false` value will be emitted via the
    completion handler.
    @see Subscription, ScopedSubscription
    @returns `true` if the subscription was found.
    @throws error::Logic if the subscription is active and not owned
            by the Session
    @note Duplicate unsubscribes using the same Subscription handle
          are safely ignored.
    @par Notable Error Codes
        - WampErrc::noSuchSubscription if the router reports that there was
          no such subscription. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<bool>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::unsubscribe(
    Subscription sub, ///< The subscription to unsubscribe from.
    C&& completion    ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(canUnsubscribe(sub),
                        "Session does not own the subscription");
    return initiate<UnsubscribeOp>(std::forward<C>(completion), sub,
                                   unspecifiedTimeout);
}

//------------------------------------------------------------------------------
/** @copydetails Session::unsubscribe(Subscription, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<bool>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::unsubscribe(
    Subscription sub, ///< The subscription to unsubscribe from.
    Timeout timeout,  ///< Timeout duration after which to disconnect.
    C&& completion    ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(canUnsubscribe(sub),
                        "Session does not own the subscription");
    return initiate<UnsubscribeOp>(std::forward<C>(completion), sub, timeout);
}

//------------------------------------------------------------------------------
/** @tparam Callable handler with signature `void (ErrorOr<PublicationId>)`,
            or a compatible Boost.Asio completion token.
    @return The publication ID for this event. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<PublicationId>, C>
#else
Session::template Deduced<ErrorOr<PublicationId>, C>
#endif
Session::publish(
    Pub pub,       ///< The publication to publish.
    C&& completion ///< Completion handler or token.
)
{
    return initiate<PublishOp>(std::forward<C>(completion), std::move(pub));
}

//------------------------------------------------------------------------------
/** @tparam S Call slot with signature `Outcome (Invocation)`
    @tparam C Callable handler with signature 'void (ErrorOr<Registration>)',
              or a compatible Boost.Asio completion token.
    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @par Notable Error Codes
        - WampErrc::procedureAlreadyExists if the router reports that the
          procedure has already been registered for this realm.
    @see @ref Registrations */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    Procedure procedure, ///< The procedure to register.
    S&& callSlot,        ///< Call invocation handler.
    C&& completion       ///< Completion handler or token.
)
{
    return initiate<EnrollOp>(
        std::forward<C>(completion), std::move(procedure),
        bindFallbackExecutor(std::forward<S>(callSlot)), nullptr);
}

//------------------------------------------------------------------------------
/** @tparam I Call slot with signature `Outcome (Interruption)`
    @copydetails Session::enroll(Procedure, S&&, C&&) */
//------------------------------------------------------------------------------
template <typename S, typename I, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    Procedure procedure, ///< The procedure to register.
    S&& callSlot,        ///< Call invocation handler.
    I&& interruptSlot,   ///< Interruption handler.
    C&& completion       ///< Completion handler or token.
    )
{
    return initiate<EnrollOp>(
        std::forward<C>(completion), std::move(procedure),
        bindFallbackExecutor(std::forward<S>(callSlot)),
        bindFallbackExecutor(std::forward<I>(interruptSlot)));
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<bool>)`,
              or a compatible Boost.Asio completion token.
    @details
    If the registration is no longer applicable, then this operation will
    effectively do nothing and a `false` value will be emitted via the
    completion handler.
    @see Registration, ScopedRegistration
    @returns `true` if the registration was found.
    @throws error::Logic if the registration is active and not owned
            by the Session
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `!!reg == true`
    @par Notable Error Codes
        - WampErrc::noSuchRegistration if the router reports that there is
          no such procedure registered by that name. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<bool>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::unregister(
    Registration reg, ///< The RPC registration to unregister.
    C&& completion    ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(canUnregister(reg),
                        "Session does not own the registration");
    return initiate<UnregisterOp>(std::forward<C>(completion), reg,
                                  unspecifiedTimeout);
}

//------------------------------------------------------------------------------
/** @copydetails Session::unregister(Registration, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<bool>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::unregister(
    Registration reg, ///< The RPC registration to unregister.
    Timeout timeout,  ///< Timeout duration after which to disconnect.
    C&& completion    ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(canUnregister(reg),
                        "Session does not own the registration");
    return initiate<UnregisterOp>(std::forward<C>(completion), reg, timeout);
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<Result>)`,
              or a compatible Boost.Asio completion token.
    @return The remote procedure result.
    @par Notable Error Codes
        - WampErrc::noSuchProcedure if the router reports that there is
          no such procedure registered by that name.
        - WampErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - WampErrc::cancelled if the call was cancelled.
        - WampErrc::timeout if the call timed out.
        - WampErrc::unavailable if the callee is unavailable.
        - WampErrc::noAvailableCallee if all registered callees are unavaible.
    @note Use Session::requestStream or Session::openStream if progressive
          results/invocations are desired. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<Result>, C>
#endif
Session::call(
    Rpc rpc,       ///< Details about the RPC.
    C&& completion ///< Completion handler or token.
)
{
    return initiate<CallOp>(std::forward<C>(completion), std::move(rpc));
}

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature 'void (CalleeChannel)'.
    @tparam C Callable handler with signature 'void (ErrorOr<Registration>)',
              or a compatible Boost.Asio completion token.
    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note The StreamSlot will be executed within the Session::strand()
          execution context and must not block. The StreamSlot is responsible
          for dispatching/posting/deferring the work to another executor if
          necessary.
    @note CalleeChannel::accept should be called within the invocation context
          of the StreamSlot in order to avoid losing incoming chunks or
          interruptions due to their respective slots not being registered in
          time.
    @par Notable Error Codes
        - WampErrc::procedureAlreadyExists if the router reports that a
          stream/procedure with the same URI has already been registered for
          this realm.
    @see @ref Streams */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    Stream stream,  ///< The stream to register.
    S&& streamSlot, ///< Stream opening handler.
    C&& completion  ///< Completion handler or token.
    )
{
    return initiate<EnrollStreamOp>(
        std::forward<C>(completion), std::move(stream),
        bindFallbackExecutor(std::forward<S>(streamSlot)));
}

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature
              `void (CallerChannel, ErrorOr<CallerInputChunk>)`.
    @tparam C Callable handler with signature `void (ErrorOr<CallerChannel::Ptr>)`,
              or a compatible Boost.Asio completion token.
    @return A new CallerChannel.
    @par Notable Error Codes
        - WampErrc::noSuchProcedure if the router reports that there is
          no such procedure/stream registered by that name.
        - WampErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - WampErrc::cancelled if the stream was cancelled.
        - WampErrc::timeout if waiting for a response timed out.
        - WampErrc::unavailable if the callee is unavailable.
        - WampErrc::noAvailableCallee if all registered callees are unavaible. */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<CallerChannel>, C>
#else
Session::template Deduced<ErrorOr<CallerChannel>, C>
#endif
Session::requestStream(
    StreamRequest req, ///< Details about the stream.
    S&& chunkSlot,     ///< Caller input chunk handler.
    C&& completion     ///< Completion handler or token.
    )
{
    return initiate<RequestStreamOp>(
        std::forward<C>(completion), std::move(req),
        bindFallbackExecutor(std::forward<S>(chunkSlot)));
}

//------------------------------------------------------------------------------
/** This overload without a `chunkSlot` can be used with unidirectional
    caller-to-callee streams.
    @copydetails Session::requestStream(StreamRequest, S&&, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<CallerChannel>, C>
#else
Session::template Deduced<ErrorOr<CallerChannel>, C>
#endif
Session::requestStream(
    StreamRequest req, ///< Details about the stream.
    C&& completion     ///< Completion handler or token.
    )
{
    return initiate<RequestStreamOp>(std::forward<C>(completion),
                                     std::move(req), nullptr);
}

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature
              `void (CallerChannel, ErrorOr<CallerInputChunk>)`.
    @tparam C Callable handler with signature `void (ErrorOr<CallerChannel::Ptr>)`,
              or a compatible Boost.Asio completion token.
    @return A new CallerChannel. */
//------------------------------------------------------------------------------
template <typename S, typename C>
Session::Deduced<ErrorOr<CallerChannel>, C> Session::openStream(
    StreamRequest req, ///< Details about the stream.
    S&& chunkSlot,     ///< Caller input chunk handler
    C&& completion     ///< Completion handler or token.
    )
{
    return initiate<OpenStreamOp>(
        std::forward<C>(completion), std::move(req),
        bindFallbackExecutor(std::forward<S>(chunkSlot)));
}

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<CallerChannel::Ptr>)`,
              or a compatible Boost.Asio completion token.
    @return A new CallerChannel. */
//------------------------------------------------------------------------------
template <typename C>
Session::Deduced<ErrorOr<CallerChannel>, C> Session::openStream(
    StreamRequest req, ///< Details about the stream.
    C&& completion     ///< Completion handler or token.
    )
{
    return initiate<OpenStreamOp>(std::forward<C>(completion),
                                  std::move(req), nullptr);
}

//------------------------------------------------------------------------------
template <typename F>
typename internal::BindFallbackExecutorResult<F>::Type
Session::bindFallbackExecutor(F&& handler) const
{
    return internal::bindFallbackExecutor(std::forward<F>(handler),
                                          fallbackExecutor_);
}

//------------------------------------------------------------------------------
template <typename O, typename C, typename... As>
Session::template Deduced<ErrorOr<typename O::ResultValue>, C>
Session::initiate(C&& token, As&&... args)
{
    return boost::asio::async_initiate<
        C, void(ErrorOr<typename O::ResultValue>)>(
        O{this, std::forward<As>(args)...}, token);
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/session.inl.hpp"
#endif

#endif // CPPWAMP_SESSION_HPP
