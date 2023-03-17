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

// TODO: Expose abort API to client apps?
// https://github.com/wamp-proto/wamp-proto/discussions/470

#include <future>
#include <memory>
#include <string>
#include <utility>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "calleestreaming.hpp"
#include "callerstreaming.hpp"
#include "config.hpp"
#include "connector.hpp"
#include "exceptions.hpp"
#include "errorcodes.hpp"
#include "erroror.hpp"
#include "logging.hpp"
#include "pubsubinfo.hpp"
#include "registration.hpp"
#include "rpcinfo.hpp"
#include "sessioninfo.hpp"
#include "subscription.hpp"
#include "tagtypes.hpp"
#include "wampdefs.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class Client; }

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
    When using Boost versions prior to 1.80.0, if one of the unpackers in
    cppwamp/corounpacker.hpp is used to register an event or RPC handler, then
    the fallback executor must originate from wamp::IoExecutor or
    wamp::AnyIoExecutor.

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
    Undecorated methods must be called within the Session's execution
    [strand](https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/strands.html).
    If the same `io_context` is used by a single-threaded app and a Session,
    calls to undecorated methods will implicitly be within the Session's strand.
    If invoked from other threads, calls to undecorated methods must be executed
    via `boost::asio::dispatch(session.stand(), operation)`. Session methods
    decorated with the ThreadSafe tag type may be safely used concurrently by
    multiple threads. These decorated methods take care of executing operations
    via a Session's strand so that they become sequential.

    @par Notable Error Codes
    - Errc::invalidState if the session was not in the appropriate state
      for a given operation
    - Errc::abandoned if an operation was aborted by the user closing
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
    // TODO: Make all public operations thread-safe?
    // Othewise, put thread-safe operations in a segregated interface
    // e.g. session.threadSafe().call(...)

private:
    struct GenericOp { template <typename F> void operator()(F&&) {} };

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
    using Deduced = decltype(
        boost::asio::async_initiate<C, void(T)>(std::declval<GenericOp&>(),
                                                std::declval<C&>()));

    /// @name Construction
    /// @{
    /** Constructor taking an executor. */
    explicit Session(Executor exec);

    /** Constructor taking an executor for I/O operations
        and another for user-provided handlers. */
    Session(const Executor& exec, FallbackExecutor fallbackExec);

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
    ~Session();
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
    /** Sets the handler that is dispatched for logging events. */
    template <typename S>
    void listenLogged(S&& logSlot);

    /** Thread-safe setting of log handler. */
    template <typename S>
    void listenLogged(ThreadSafe, S&& logSlot);

    /** Sets the maximum level of log events that will be emitted. */
    void setLogLevel(LogLevel level);

    /** Sets the handler that is posted for session state changes. */
    template <typename S>
    void listenStateChanged(S&& stateSlot);

    /** Thread-safe setting of state change handler. */
    template <typename S>
    void listenStateChanged(ThreadSafe, S&& stateSlot);
    /// @}

    /// @name Session Management
    /// @{
    /** Asynchronously attempts to connect to a router. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(ConnectionWish wish, C&& completion);

    /** Thread-safe connect. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(ThreadSafe, ConnectionWish wish, C&& completion);

    /** Asynchronously attempts to connect to a router. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(ConnectionWishList wishes, C&& completion);

    /** Thread-safe connect. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(ThreadSafe, ConnectionWishList wishes, C&& completion);

    /** Asynchronously attempts to join the given WAMP realm. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Welcome>, C>
    join(Realm realm, C&& completion);

    /** Thread-safe join. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Welcome>, C>
    join(ThreadSafe, Realm realm, C&& completion);

    /** Asynchronously attempts to join the given WAMP realm, using the given
        authentication challenge handler. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Welcome>, C>
    join(Realm realm, S&& challengeSlot, C&& completion);

    /** Thread-safe join with challenge handler. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Welcome>, C>
    join(ThreadSafe, Realm realm, S&& challengeSlot, C&& completion);

    /** Asynchronously leaves the WAMP session. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Reason>, C>
    leave(C&& completion);

    /** Thread-safe leave. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Reason>, C>
    leave(ThreadSafe, C&& completion);

    /** Asynchronously leaves the WAMP session with the given reason. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Reason>, C>
    leave(Reason reason, C&& completion);

    /** Thread-safe leave with reason. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Reason>, C>
    leave(ThreadSafe, Reason reason, C&& completion);

    /** Disconnects the transport between the client and router. */
    void disconnect();

    /** Thread-safe disconnect. */
    void disconnect(ThreadSafe);

    /** Terminates the transport connection between the client and router. */
    void terminate();

    /** Thread-safe reset. */
    void terminate(ThreadSafe);
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Subscription>, C>
    subscribe(Topic topic, S&& eventSlot, C&& completion);

    /** Thread-safe subscribe. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Subscription>, C>
    subscribe(ThreadSafe, Topic topic, S&& eventSlot, C&& completion);

    /** Unsubscribes a subscription to a topic. */
    void unsubscribe(Subscription sub);

    /** Thread-safe unsubscribe. */
    void unsubscribe(ThreadSafe, Subscription sub);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement, if necessary. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unsubscribe(Subscription sub, C&& completion);

    /** Thread-safe acknowledged unsubscribe. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unsubscribe(ThreadSafe, Subscription sub, C&& completion);

    /** Publishes an event. */
    CPPWAMP_NODISCARD ErrorOrDone publish(Pub pub);

    /** Thread-safe publish. */
    CPPWAMP_NODISCARD std::future<ErrorOrDone> publish(ThreadSafe, Pub pub);

    /** Publishes an event and waits for an acknowledgement from the router. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<PublicationId>, C>
    publish(Pub pub, C&& completion);

    /** Thread-safe acknowledged publish. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<PublicationId>, C>
    publish(ThreadSafe, Pub pub, C&& completion);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Procedure procedure, S&& callSlot, C&& completion);

    /** Thread-safe enroll. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(ThreadSafe, Procedure procedure, S&& callSlot, C&& completion);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    template <typename S, typename I, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Procedure procedure, S&& callSlot, I&& interruptSlot,
           C&& completion);

    /** Thread-safe enroll interruptible. */
    template <typename S, typename I, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(ThreadSafe, Procedure procedure, S&& callSlot, I&& interruptSlot,
           C&& completion);

    /** Unregisters a remote procedure call. */
    void unregister(Registration reg);

    /** Thread-safe unregister. */
    void unregister(ThreadSafe, Registration reg);

    /** Unregisters a remote procedure call and waits for router
        acknowledgement. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unregister(Registration reg, C&& completion);

    /** Thread-safe acknowledged unregister. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    unregister(ThreadSafe, Registration reg, C&& completion);

    /** Calls a remote procedure. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(Rpc rpc, C&& completion);

    /** Thread-safe call. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(ThreadSafe, Rpc rpc, C&& completion);
    /// @}

    /// @name Streaming
    /// @{
    /** Registers a streaming endpoint. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Stream stream, S&& streamSlot, C&& completion);

    /** Thread-safe enroll stream. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(ThreadSafe, Stream stream, S&& streamSlot, C&& completion);

    /** Sends a request to open a stream and waits for an initial response. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<CallerChannel>, C>
    requestStream(StreamRequest req, S&& chunkSlot, C&& completion);

    /** Sends a request to open a stream and waits for an initial response. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<CallerChannel>, C>
    requestStream(StreamRequest req, C&& completion);

    /** Thread-safe requestStream. */
    template <typename S, typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<CallerChannel>, C>
    requestStream(ThreadSafe, StreamRequest req, S&& chunkSlot, C&& completion);

    /** Opens a streamming channel without negotiation. */
    template <typename S>
    CPPWAMP_NODISCARD ErrorOr<CallerChannel>
    openStream(StreamRequest req, S&& chunkSlot = {});

    /** Thread-safe openStream. */
    template <typename S>
    CPPWAMP_NODISCARD std::future<ErrorOr<CallerChannel>>
    openStream(ThreadSafe, StreamRequest req, S&& onChunk = {});
    /// @}

private:
    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<T>)>;

    using LogSlot       = AnyReusableHandler<void (LogEntry)>;
    using StateSlot     = AnyReusableHandler<void (State, std::error_code ec)>;
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

    template <typename S>
    typename internal::BindFallbackExecutorResult<S>::Type
    bindFallbackExecutor(S&& slot) const;

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    initiate(C&& token, As&&... args);

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    safelyInitiate(C&& token, As&&... args);

    void doSetLogHandler(LogSlot&& s);
    void safeListenLogged(LogSlot&& s);
    void doSetStateChangeHandler(StateSlot&& s);
    void safeListenStateChanged(ThreadSafe, StateSlot&& s);
    void doConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f);
    void safeConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f);
    void doJoin(Realm&& r, ChallengeSlot&& s, CompletionHandler<Welcome>&& f);
    void safeJoin(Realm&& r, ChallengeSlot&& c,
                  CompletionHandler<Welcome>&& f);
    void doLeave(Reason&& reason, CompletionHandler<Reason>&& f);
    void safeLeave(Reason&& r, CompletionHandler<Reason>&& f);
    void doSubscribe(Topic&& t, EventSlot&& s,
                     CompletionHandler<Subscription>&& f);
    void safeSubscribe(Topic&& t, EventSlot&& s,
                       CompletionHandler<Subscription>&& f);
    void doUnsubscribe(const Subscription& s, CompletionHandler<bool>&& f);
    void safeUnsubscribe(const Subscription& s, CompletionHandler<bool>&& f);
    void doPublish(Pub&& p, CompletionHandler<PublicationId>&& f);
    void safePublish(Pub&& p, CompletionHandler<PublicationId>&& f);
    void doEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                  CompletionHandler<Registration>&& f);
    void safeEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                    CompletionHandler<Registration>&& f);
    void doUnregister(const Registration& r, CompletionHandler<bool>&& f);
    void safeUnregister(const Registration& r, CompletionHandler<bool>&& f);
    void doCall(Rpc&& r, CompletionHandler<Result>&& f);
    void safeCall(Rpc&& r, CompletionHandler<Result>&& f);
    void doEnroll(Stream&& s, StreamSlot&& ss,
                  CompletionHandler<Registration>&& f);
    void safeEnroll(Stream&& s, StreamSlot&& ss,
                    CompletionHandler<Registration>&& f);
    void doRequestStream(StreamRequest&& r, CallerChunkSlot&& c,
                         CompletionHandler<CallerChannel>&& f);
    void safeRequestStream(StreamRequest&& r, CallerChunkSlot&& c,
                           CompletionHandler<CallerChannel>&& f);
    ErrorOr<CallerChannel> doOpenStream(StreamRequest&& r, CallerChunkSlot&& s);
    std::future<ErrorOr<CallerChannel> > safeOpenStream(StreamRequest&& r,
                                                        CallerChunkSlot&& s);

    FallbackExecutor fallbackExecutor_;
    std::shared_ptr<internal::Client> impl_;
};


//******************************************************************************
// Session template function implementations
//******************************************************************************

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature `void (LogEntry)`
    @details
    Log events are emitted in the following situations:
    - Errors: Protocol violations, message deserialization errors, unsupported
              features, invalid states, inability to perform operations,
              conversion errors, or transport payload overflows.
    - Warnings: Problems that do not prevent operations from proceeding.
    - Traces: Transmitted and received WAMP messages presented in JSON format.

    Log events are discarded when there is no log handler set.

    Copies of the handler are made when they are dispatched. If the handler
    needs to be stateful, or is non-copyable, then pass a stateless copyable
    proxy instead.

    @note No state change events are fired when the session object is
          terminating.
    @see Session::setLogLevel */
//------------------------------------------------------------------------------
template <typename S>
void Session::listenLogged(S&& logSlot)
{
    doSetLogHandler(bindFallbackExecutor(std::forward<S>(logSlot)));
}

//------------------------------------------------------------------------------
/** @copydetails Session::listenLogged(S&&) */
//------------------------------------------------------------------------------
template <typename S>
void Session::listenLogged(ThreadSafe, S&& logSlot)
{
    safeListenLogged(bindFallbackExecutor(std::forward<S>(logSlot)));
}

//------------------------------------------------------------------------------
/** @details
    Copies of the handler are made when they are dispatched. If the handler
    needs to be stateful, or is non-copyable, then pass a stateless copyable
    proxy instead.

    @note No state change events are fired when the session object is
          terminating. */
//------------------------------------------------------------------------------
template <typename S>
CPPWAMP_INLINE void Session::listenStateChanged(S&& stateSlot)
{
    doSetStateChangeHandler(std::move(stateSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::listenStateChanged(S&&) */
//------------------------------------------------------------------------------
template <typename S>
CPPWAMP_INLINE void Session::listenStateChanged(ThreadSafe, S&& stateSlot)
{
    safeListenStateChanged(std::move(stateSlot));
}

//------------------------------------------------------------------------------
struct Session::ConnectOp
{
    using ResultValue = size_t;
    Session* self;
    ConnectionWishList w;

    template <typename F> void operator()(F&& f)
    {
        self->doConnect(std::move(w), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeConnect(std::move(w), std::forward<F>(f));
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
/** @copydetails Session::connect(ConnectionWishList, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<std::size_t>, C>
#else
Session::template Deduced<ErrorOr<std::size_t>, C>
#endif
Session::connect(
    ThreadSafe,
    ConnectionWish wish, /**< Transport/codec combination to use for
                              attempting connection. */
    C&& completion       /**< Completion handler or token. */
    )
{
    return connect(threadSafe, ConnectionWishList{std::move(wish)},
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
/** @copydetails Session::connect(ConnectionWishList, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<std::size_t>, C>
#else
Session::template Deduced<ErrorOr<std::size_t>, C>
#endif
Session::connect(
    ThreadSafe,
    ConnectionWishList wishes, /**< List of transport addresses/options/codecs
                                    to use for attempting connection. */
    C&& completion             /**< Completion handler or token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!wishes.empty(),
                        "Session::connect ConnectionWishList cannot be empty");
    return safelyInitiate<ConnectOp>(std::forward<C>(completion),
                                     std::move(wishes));
}

//------------------------------------------------------------------------------
struct Session::JoinOp
{
    using ResultValue = Welcome;
    Session* self;
    Realm r;
    ChallengeSlot s;

    template <typename F> void operator()(F&& f)
    {
        self->doJoin(std::move(r), std::move(s), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeJoin(threadSafe, std::move(r), std::move(s),
                       std::forward<F>(f));
    }
};

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
    Realm realm,   ///< Details on the realm to join.
    C&& completion ///< Completion handler or token.
    )
{
    return initiate<JoinOp>(std::forward<C>(completion), std::move(realm),
                            nullptr);
}

//------------------------------------------------------------------------------
/** @copydetails Session::join(Realm, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Welcome>, C>
#else
Session::template Deduced<ErrorOr<Welcome>, C>
#endif
Session::join(
    ThreadSafe,
    Realm realm,   ///< Details on the realm to join.
    C&& completion ///< Completion handler or token.
    )
{
    return safelyInitiate<JoinOp>(std::forward<C>(completion),
                                  std::move(realm), nullptr);
}

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature `void (Challenge)`
    @copydetails Session::join(Realm, C&&)
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
    Realm realm,       /**< Details on the realm to join. */
    S&& challengeSlot, /**< Handles authentication challenges. */
    C&& completion     /**< Completion handler or token. */
    )
{
    return initiate<JoinOp>(
        std::forward<C>(completion), std::move(realm),
        std::forward<S>(challengeSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::join(Realm, S&&, C&&) */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Welcome>, C>
#else
Session::template Deduced<ErrorOr<Welcome>, C>
#endif
Session::join(
    ThreadSafe,
    Realm realm,       /**< Details on the realm to join. */
    S&& challengeSlot, /**< Handles authentication challenges. */
    C&& completion     /**< Completion handler or token. */
    )
{
    return safelyInitiate<JoinOp>(
        std::forward<C>(completion), std::move(realm),
        std::forward<S>(challengeSlot));
}

//------------------------------------------------------------------------------
struct Session::LeaveOp
{
    using ResultValue = Reason;
    Session* self;
    Reason r;

    template <typename F> void operator()(F&& f)
    {
        self->doLeave(std::move(r), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeLeave(std::move(r), std::forward<F>(f));
    }
};

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
    // TODO: Timeout
    return leave(Reason{WampErrc::closeRealm}, std::forward<C>(completion));
}

//------------------------------------------------------------------------------
/** @copydetails Session::leave(C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<Reason>, C>
#endif
Session::leave(
    ThreadSafe,
    C&& completion ///< Completion handler or token.
    )
{
    return leave(threadSafe, Reason("wamp.close.close_realm"),
                 std::forward<C>(completion));
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
    Reason reason, ///< %Reason URI and other options
    C&& completion ///< Completion handler or token.
    )
{
    return initiate<LeaveOp>(std::forward<C>(completion), std::move(reason));
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
    ThreadSafe,
    Reason reason, ///< %Reason URI and other options
    C&& completion ///< Completion handler.
    )
{
    return safelyInitiate<LeaveOp>(std::forward<C>(completion),
                                   std::move(reason));
}

//------------------------------------------------------------------------------
struct Session::SubscribeOp
{
    using ResultValue = Subscription;
    Session* self;
    Topic t;
    EventSlot s;

    template <typename F> void operator()(F&& f)
    {
        self->doSubscribe(std::move(t), std::move(s), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeSubscribe(std::move(t), std::move(s), std::forward<F>(f));
    }
};

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
        std::forward<S>(eventSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::subscribe(Topic, S&&, C&&) */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Subscription>, C>
#else
Session::template Deduced<ErrorOr<Subscription>, C>
#endif
Session::subscribe(
    ThreadSafe,
    Topic topic,   ///< The topic to subscribe to.
    S&& eventSlot, ///< Event handler.
    C&& completion ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(topic.matchPolicy() != MatchPolicy::unknown,
                        "Unsupported match policy for subscribe operation");
    return safelyInitiate<SubscribeOp>(
        std::forward<C>(completion), std::move(topic),
        std::forward<S>(eventSlot));
}

//------------------------------------------------------------------------------
struct Session::UnsubscribeOp
{
    using ResultValue = bool;
    Session* self;
    Subscription s;

    template <typename F> void operator()(F&& f)
    {
        self->doUnsubscribe(std::move(s), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeUnsubscribe(std::move(s), std::forward<F>(f));
    }
};

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
    @note Duplicate unsubscribes using the same Subscription handle
          are safely ignored.
    @pre `!!sub == true`
    @par Notable Error Codes
        - WampErrc::noSuchSubscription if the router reports that there was
          no such subscription.
    @throws error::Logic if the given subscription is empty */
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
    CPPWAMP_LOGIC_CHECK(bool(sub), "The subscription is empty");
    return initiate<UnsubscribeOp>(std::forward<C>(completion), sub);
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
    ThreadSafe,
    Subscription sub, ///< The subscription to unsubscribe from.
    C&& completion    ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(bool(sub), "The subscription is empty");
    return safelyInitiate<UnsubscribeOp>(std::forward<C>(completion), sub);
}

//------------------------------------------------------------------------------
struct Session::PublishOp
{
    using ResultValue = PublicationId;
    Session* self;
    Pub p;

    template <typename F> void operator()(F&& f)
    {
        self->doPublish(std::move(p), std::move(std::forward<F>(f)));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safePublish(std::move(p), std::move(std::forward<F>(f)));
    }
};

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
/** @copydetails Session::publish(Pub, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<PublicationId>, C>
#else
Session::template Deduced<ErrorOr<PublicationId>, C>
#endif
Session::publish(
    ThreadSafe,
    Pub pub,       ///< The publication to publish.
    C&& completion ///< Completion handler or token.
    )
{
    return safelyInitiate<PublishOp>(std::forward<C>(completion),
                                     std::move(pub));
}

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
                       std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeEnroll(std::move(p), std::move(s), std::move(i),
                         std::forward<F>(f));
    }
};

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
        std::forward<S>(callSlot), nullptr);
}

//------------------------------------------------------------------------------
/** @copydetails Session::enroll(Procedure, S&&, C&&) */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    ThreadSafe,
    Procedure procedure, ///< The procedure to register.
    S&& callSlot,        ///< Call invocation handler.
    C&& completion       ///< Completion handler or token.
    )
{
    return safelyInitiate<EnrollOp>(
        std::forward<C>(completion), std::move(procedure),
        std::forward<S>(callSlot), nullptr);
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
        std::forward<S>(callSlot), std::forward<I>(interruptSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::enroll(Procedure, S&&, I&&, C&&) */
//------------------------------------------------------------------------------
template <typename S, typename I, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    ThreadSafe,
    Procedure procedure, ///< The procedure to register.
    S&& callSlot,        ///< Call invocation handler.
    I&& interruptSlot,   ///< Interruption handler.
    C&& completion       ///< Completion handler or token.
    )
{
    return safelyInitiate<EnrollOp>(
        std::forward<C>(completion), std::move(procedure),
        std::forward<S>(callSlot), std::forward<S>(interruptSlot));
}

//------------------------------------------------------------------------------
struct Session::UnregisterOp
{
    using ResultValue = bool;
    Session* self;
    Registration r;

    template <typename F> void operator()(F&& f)
    {
        self->doUnregister(std::move(r), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeUnregister(std::move(r), std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<bool>)`,
              or a compatible Boost.Asio completion token.
    @details
    If the registration is no longer applicable, then this operation will
    effectively do nothing and a `false` value will be emitted via the
    completion handler.
    @see Registration, ScopedRegistration
    @returns `true` if the registration was found.
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
    CPPWAMP_LOGIC_CHECK(bool(reg), "The registration is empty");
    return initiate<UnregisterOp>(std::forward<C>(completion), reg);
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
    ThreadSafe,
    Registration reg, ///< The RPC registration to unregister.
    C&& completion    ///< Completion handler or token.
    )
{
    CPPWAMP_LOGIC_CHECK(bool(reg), "The registration is empty");
    return safelyInitiate<UnregisterOp>(std::forward<C>(completion), reg);
}

//------------------------------------------------------------------------------
struct Session::CallOp
{
    using ResultValue = Result;
    Session* self;
    Rpc r;

    template <typename F> void operator()(F&& f)
    {
        self->doCall(std::move(r), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeCall(std::move(r), std::forward<F>(f));
    }
};

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
/** @copydetails Session::call(Rpc, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<Result>, C>
#endif
Session::call(
    ThreadSafe,
    Rpc rpc,       ///< Details about the RPC.
    C&& completion ///< Completion handler or token.
    )
{
    return safelyInitiate<CallOp>(std::forward<C>(completion), std::move(rpc));
}

//------------------------------------------------------------------------------
struct Session::EnrollStreamOp
{
    using ResultValue = Registration;
    Session* self;
    Stream s;
    StreamSlot ss;

    template <typename F> void operator()(F&& f)
    {
        self->doEnroll(std::move(s), std::move(ss), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeEnroll(std::move(s), std::move(ss), std::forward<F>(f));
    }
};

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
          of the StreamSlot in order to losing incoming chunks or interruptions
          due to their respective slots not being registered in time.
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
        std::forward<S>(streamSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::enroll(Stream, S&&, C&&) */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    ThreadSafe,
    Stream stream,  ///< The stream to register.
    S&& streamSlot, ///< Stream opening handler.
    C&& completion  ///< Completion handler or token.
    )
{
    return safelyInitiate<EnrollStreamOp>(
        std::forward<C>(completion), std::move(stream), std::move(streamSlot));
}

//------------------------------------------------------------------------------
struct Session::RequestStreamOp
{
    using ResultValue = CallerChannel;
    Session* self;
    StreamRequest r;
    CallerChunkSlot c;

    template <typename F> void operator()(F&& f)
    {
        self->doRequestStream(std::move(r), std::move(c), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeRequestStream(std::move(r), std::move(c), std::forward<F>(f));
    }
};

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
        std::forward<S>(chunkSlot));
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
/** @copydetails Session::requestStream(StreamRequest, S&&, C&&) */
//------------------------------------------------------------------------------
template <typename S, typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<CallerChannel>, C>
#endif
Session::requestStream(
    ThreadSafe,
    StreamRequest req, ///< Details about the stream.
    S&& chunkSlot,     ///< Caller input chunk handler.
    C&& completion     ///< Completion handler or token.
    )
{
    return safelyInitiate<RequestStreamOp>(
        std::forward<C>(completion), std::move(req),
        std::forward<S>(chunkSlot));
}

//------------------------------------------------------------------------------
/** @tparam S Callable handler with signature
              `void (CallerChannel, ErrorOr<CallerInputChunk>)`.
    @return A new CallerChannel. */
//------------------------------------------------------------------------------
template <typename S>
ErrorOr<CallerChannel> Session::openStream(
    StreamRequest req, ///< Details about the stream.
    S&& chunkSlot      ///< Caller input chunk handler
    )
{
    return doOpenStream(std::move(req), std::forward<S>(chunkSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::openStream(StreamRequest, S&&) */
//------------------------------------------------------------------------------
template <typename S>
CPPWAMP_INLINE std::future<ErrorOr<CallerChannel>> Session::openStream(
    ThreadSafe,
    StreamRequest req, ///< Details about the stream.
    S&& chunkSlot      ///< Caller input chunk handler
    )
{
    return safeOpenStream(std::move(req), std::forward<S>(chunkSlot));
}

//------------------------------------------------------------------------------
template <typename S>
typename internal::BindFallbackExecutorResult<S>::Type
Session::bindFallbackExecutor(S&& slot) const
{
    return internal::bindFallbackExecutor(std::forward<S>(slot),
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

//------------------------------------------------------------------------------
template <typename O, typename C, typename... As>
Session::template Deduced<ErrorOr<typename O::ResultValue>, C>
Session::safelyInitiate(C&& token, As&&... args)
{
    return boost::asio::async_initiate<
        C, void(ErrorOr<typename O::ResultValue>)>(
        O{this, std::forward<As>(args)...}, token, threadSafe);
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/session.ipp"
#endif

#endif // CPPWAMP_SESSION_HPP
