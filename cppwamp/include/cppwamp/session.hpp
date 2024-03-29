/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSION_HPP
#define CPPWAMP_SESSION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the asynchronous session API used by a _client_ peer
           in WAMP applications. */
//------------------------------------------------------------------------------

#include <future>
#include <memory>
#include <string>
#include <utility>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "chits.hpp"
#include "config.hpp"
#include "connector.hpp"
#include "erroror.hpp"
#include "logging.hpp"
#include "peerdata.hpp"
#include "registration.hpp"
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

    @see ErrorOr, Registration, Subscription. */
//------------------------------------------------------------------------------
class CPPWAMP_API Session
{
private:
    struct GenericOp { template <typename F> void operator()(F&&) {} };

public:
    /** Shared pointer to a Session.
        @deprecated will be removed */
    using Ptr = std::shared_ptr<Session>;

    /** Executor type used for I/O operations. */
    using Executor = AnyIoExecutor;

    /** Fallback executor type for user-provided handlers. */
    using FallbackExecutor = AnyCompletionExecutor;

    /** Enumerates the possible states that a Session can be in. */
    using State = SessionState;

    /** Type-erased wrapper around a WAMP event handler. */
    using EventSlot = AnyReusableHandler<void (Event)>;

    /** Type-erased wrapper around an RPC handler. */
    using CallSlot = AnyReusableHandler<Outcome (Invocation)>;

    /** Type-erased wrapper around an RPC interruption handler. */
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;

    /** Type-erased wrapper around a log event handler. */
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    /** Type-erased wrapper around a log string event handler. */
    // TODO: Remove
    using LogStringHandler = AnyReusableHandler<void (std::string)>;

    /** Type-erased wrapper around a Session state change handler. */
    using StateChangeHandler = AnyReusableHandler<void (SessionState)>;

    /** Type-erased wrapper around an authentication challenge handler. */
    using ChallengeHandler = AnyReusableHandler<void (Challenge)>;

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
    /** Creates a new Session instance on the heap.
        @deprecated Stack allocation is now permitted. */
    static Ptr create(AnyIoExecutor exec);

    /** Creates a new Session instance on the heap. */
    static Ptr create(const AnyIoExecutor& exec, FallbackExecutor fallbackExec);

    /** Creates a new Session instance on the heap.
        @deprecated Stack allocation is now permitted.
        @copydetails Session::create(Executor)
        @details Only participates in overload resolution when
                 `isExecutionContext<E>() == true`
        @tparam E Must meet the requirements of Boost.Asio's ExecutionContext */
    template <typename E>
    CPPWAMP_DEPRECATED static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<E>())
    create(
        E& executionContext /**< Context providing the executor from which
                                 Session will extract a strand for its
                                 internal I/O operations. */
    )
    {
        return create(executionContext.get_executor());
    }

    /** Creates a new Session instance on the heap.
        @deprecated Stack allocation is now permitted.
        @copydetails Session::create(Executor)
        @details Only participates in overload resolution when
                 `isExecutionContext<E1>() && isExecutionContext<E1>() == true`
        @tparam E1 Must meet the requirements of Boost.Asio's ExecutionContext
        @tparam E2 Must meet the requirements of Boost.Asio's ExecutionContext */
    template <typename E1, typename E2>
    CPPWAMP_DEPRECATED static
        CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<E1>() &&
                                  isExecutionContext<E2>())
    create(
        E1& context,        /**< Context providing the executor from which
                                 Session will extract a strand for
                                 its internal I/O operations. */
        E1& fallbackContext /**< Context providing the executor which serves
                                 as fallback for all user-provided handlers. */
    )
    {
        return create(context.get_executor(), fallbackContext.get_executor());
    }

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead.
        @deprecated Stack allocation is now permitted. */
    CPPWAMP_DEPRECATED static Ptr create(FallbackExecutor fallbackExec,
                                         LegacyConnector connector);

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead.
        @deprecated Stack allocation is now permitted. */
    CPPWAMP_DEPRECATED static Ptr create(FallbackExecutor fallbackExec,
                                         ConnectorList connectors);

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead.
        @deprecated Stack allocation is now permitted.
        @copydetails Session::create(FallbackExecutor, LegacyConnector)
        @details Only participates in overload resolution when
                 `isExecutionContext<TExecutionContext>() == true`
        @tparam TExecutionContext Must meet the requirements of
                                  Boost.Asio's ExecutionContext */
    template <typename E>
    CPPWAMP_DEPRECATED static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<E>())
    create(
        E& fallbackContext,  /**< Context providing the executor which serves
                                  as fallback for all user-provided handlers. */
        LegacyConnector connector /**< Connection details for the
                                       transport to use. */
        )
    {
        return create(fallbackContext.get_executor(), std::move(connector));
    }

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead.
        @deprecated Stack allocation is now permitted.
        @copydetails Session::create(FallbackExecutor, ConnectorList)
        @details Only participates in overload resolution when
                 `isExecutionContext<E>() == true`
        @tparam E Must meet the requirements of Boost.Asio's ExecutionContext */
    template <typename E>
    CPPWAMP_DEPRECATED static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<E>())
    create(
        E& fallbackContext, /**< Context providing the executor which serves
                                 as fallback for all user-provided handlers. */
        ConnectorList connectors  /**< Connection details for the
                                       transport to use. */
    )
    {
        return create(fallbackContext.get_executor(), std::move(connectors));
    }

    /** Constructor taking an executor. */
    explicit Session(Executor exec);

    /** Constructor taking an executor for I/O operations
        and another for user-provided handlers. */
    Session(const Executor& exec, FallbackExecutor fallbackExec);

    /** Constructor taking an execution context. */
    template <typename E, EnableIf<isExecutionContext<E>()> = 0>
    explicit Session(E& context) : Session(context.get_executor()) {}

    /** Constructor taking an I/O execution context and another as fallback
        for user-provided handlers. */
    template <typename E1, typename E2,
             EnableIf<isExecutionContext<E1>() && isExecutionContext<E2>()> = 0>
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

    /** Obtains a dictionary of roles and features supported on the client
        side. */
    static const Object& roles();

    /** Obtains the execution context in which which I/O operations are
        serialized. */
    const IoStrand& strand() const;

    /** Obtains the fallback executor used for user-provided handlers. */
    FallbackExecutor fallbackExecutor() const;

    /** @deprecated Use Session::fallbackExecutor instead */
    CPPWAMP_DEPRECATED FallbackExecutor userExecutor() const;

    /** Legacy function kept for backward compatiblity. */
    CPPWAMP_DEPRECATED FallbackExecutor userIosvc() const;

    /** Returns the current state of the session. */
    SessionState state() const;
    /// @}

    /// @name Modifiers
    /// @{
    /** Sets the handler that is dispatched for logging events. */
    void setLogHandler(LogHandler handler);

    /** Thread-safe setting of log handler. */
    void setLogHandler(ThreadSafe, LogHandler handler);

    /** Sets the maximum level of log events that will be emitted. */
    void setLogLevel(LogLevel level);

    /** Sets the log handler that is dispatched for warnings. */
    CPPWAMP_DEPRECATED void setWarningHandler(LogStringHandler handler);

    /** Thread-safe setting of warning handler. */
    CPPWAMP_DEPRECATED void setWarningHandler(ThreadSafe,
                                              LogStringHandler handler);

    /** Sets the log handler that is dispatched for debug traces. */
    CPPWAMP_DEPRECATED void setTraceHandler(LogStringHandler handler);

    /** Thread-safe setting of trace handler. */
    CPPWAMP_DEPRECATED void setTraceHandler(ThreadSafe,
                                            LogStringHandler handler);

    /** Sets the handler that is posted for session state changes. */
    void setStateChangeHandler(StateChangeHandler handler);

    /** Thread-safe setting of state change handler. */
    void setStateChangeHandler(ThreadSafe, StateChangeHandler handler);

    /** Sets the handler that is dispatched for authentication challenges. */
    void setChallengeHandler(ChallengeHandler handler);

    /** Thread-safe setting of state change handler. */
    void setChallengeHandler(ThreadSafe, ChallengeHandler handler);
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

    /** Asynchronously attempts to connect to a router using the legacy
        connectors passed during session creation.
        @deprecated Use a connect overload taking wishes instead. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(C&& completion);

    /** Thread-safe legacy connect.
        @deprecated Use a connect overload taking wishes instead. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    connect(ThreadSafe, C&& completion);

    /** Asynchronously attempts to join the given WAMP realm. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SessionInfo>, C>
    join(Realm realm, C&& completion);

    /** Thread-safe join. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SessionInfo>, C>
    join(ThreadSafe, Realm realm, C&& completion);

    /** Sends an `AUTHENTICATE` in response to a `CHALLENGE`. */
    CPPWAMP_NODISCARD ErrorOrDone authenticate(Authentication auth);

    /** Thread-safe authenticate. */
    CPPWAMP_NODISCARD std::future<ErrorOrDone>
    authenticate(ThreadSafe, Authentication auth);

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

    /** @deprecated Use Session::terminate instead. */
    void reset();

    /** @deprecated Use Session::terminate instead. */
    void reset(ThreadSafe);
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Subscription>, C>
    subscribe(Topic topic, EventSlot eventSlot, C&& completion);

    /** Thread-safe subscribe. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Subscription>, C>
    subscribe(ThreadSafe, Topic topic, EventSlot eventSlot, C&& completion);

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
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Procedure procedure, CallSlot callSlot, C&& completion);

    /** Thread-safe enroll. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(ThreadSafe, Procedure procedure, CallSlot callSlot, C&& completion);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(Procedure procedure, CallSlot callSlot, InterruptSlot interruptSlot,
           C&& completion);

    /** Thread-safe enroll interruptible. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Registration>, C>
    enroll(ThreadSafe, Procedure procedure, CallSlot callSlot,
           InterruptSlot interruptSlot, C&& completion);

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

    /** Calls a remote procedure, obtaining a token that can be used
        for cancellation. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(Rpc rpc, CallChit& chit, C&& completion);

    /** Thread-safe call with CallChit capture. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion);

    /** Calls a remote procedure with progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(Rpc rpc, C&& completion);

    /** Thread-safe call with progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(ThreadSafe, Rpc rpc, C&& completion);

    /** Calls a remote procedure with progressive results, obtaining a token
        that can be used for cancellation. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(Rpc rpc, CallChit& chit, C&& completion);

    /** Thread-safe call with CallChit capture and progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion);

    /** Cancels a remote procedure using the cancel mode that was specified
        in the @ref wamp::Rpc "Rpc". */
    ErrorOrDone cancel(CallChit);

    /** Thread-safe cancel. */
    std::future<ErrorOrDone> cancel(ThreadSafe, CallChit);

    /** Cancels a remote procedure using the given mode. */
    ErrorOrDone cancel(CallChit, CallCancelMode mode);

    /** Thread-safe cancel with a given mode. */
    std::future<ErrorOrDone> cancel(ThreadSafe, CallChit, CallCancelMode mode);

    /** Cancels a remote procedure.
        @deprecated Use the overload taking a CallChit. */
    void cancel(CallCancellation cancellation);

    /** Thread-safe cancel.
        @deprecated Use the overload taking a CallChit. */
    void cancel(ThreadSafe, CallCancellation cancellation);
    /// @}

private:
    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<T>)>;

    template <typename T>
    using ReusableHandler = AnyReusableHandler<void(T)>;

    using OngoingCallHandler = AnyReusableHandler<void(ErrorOr<Result>)>;

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

    CPPWAMP_HIDDEN explicit Session(FallbackExecutor fallbackExec,
                                    ConnectorList connectors);

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    initiate(C&& token, As&&... args);

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    safelyInitiate(C&& token, As&&... args);

    void doConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f);
    void safeConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f);
    void doJoin(Realm&& realm, CompletionHandler<SessionInfo>&& f);
    void safeJoin(Realm&& r, CompletionHandler<SessionInfo>&& f);
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
    void doOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f);
    void safeOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f);
    void doOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f);
    void safeOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f);

    ConnectorList legacyConnectors_;
    std::shared_ptr<internal::Client> impl_;

    // TODO: Remove this once CoroSession is removed
    template <typename> friend class CoroSession;
};


//******************************************************************************
// Session template function implementations
//******************************************************************************

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
/** @details
    The session will attempt to connect using the transport/codec combinations
    specified in the given ConnectionWishList, in the same order.
    @return The index of the ConnectionWish used to establish the connetion
            (always zero for this overload).
    @post `this->state() == SessionState::connecting` if successful
    @par Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - SessionErrc::allTransportsFailed if more than one transport was
          specified and they all failed to connect.
        - SessionErrc::invalidState if the session was not disconnected
          during attempt to connect.
        - Some other platform or transport-dependent `std::error_code` if
          only one transport was specified and it failed to connect. */
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
    C&& completion /**< A callable handler of type `void(ErrorOr<size_t>)`,
                        or a compatible Boost.Asio completion token. */
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
    C&& completion /**< A callable handler of type void(ErrorOr<size_t>),
                        or a compatible Boost.Asio completion token. */
    )
{
    return connect(threadSafe, ConnectionWishList{std::move(wish)},
                   std::forward<C>(completion));
}

//------------------------------------------------------------------------------
/** @details
    The session will attempt to connect using the transport/codec combinations
    specified in the given ConnectionWishList, in the same order.
    @return The index of the ConnectionWish used to establish the connetion.
    @pre `wishes.empty() == false`
    @post `this->state() == SessionState::connecting` if successful
    @throws error::Logic if the given wish list is empty
    @par Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - SessionErrc::allTransportsFailed if more than one transport was
          specified and they all failed to connect.
        - SessionErrc::invalidState if the session was not disconnected
          during the attempt to connect.
        - Some other platform or transport-dependent `std::error_code` if
          only one transport was specified and it failed to connect. */
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
    C&& completion /**< A callable handler of type `void(ErrorOr<size_t>)`,
                        or a compatible Boost.Asio completion token. */
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
    C&& completion /**< A callable handler of type void(ErrorOr<size_t>),
                        or a compatible Boost.Asio completion token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!wishes.empty(),
                        "Session::connect ConnectionWishList cannot be empty");
    return safelyInitiate<ConnectOp>(std::forward<C>(completion),
                                     std::move(wishes));
}

//------------------------------------------------------------------------------
/** @details
    The session will attempt to connect using the transports that were
    specified by the wamp::LegacyConnect objects passed during create().
    If more than one transport was specified, they will be traversed in the
    same order as they appeared in the @ref ConnectorList.
    @return The index of the Connecting object used to establish the connetion.
    @pre The Session::create overload taking legacy connectors was used.
    @post `this->state() == SessionState::connecting` if successful
    @throws error::Logic if the Session::create overload taking legacy
            connectors was not used.
    @par Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - SessionErrc::allTransportsFailed if more than one transport was
          specified and they all failed to connect.
        - SessionErrc::invalidState if the session was not disconnected
          during the attempt to connect.
        - Some other platform or transport-dependent `std::error_code` if
          only one transport was specified and it failed to connect. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<std::size_t>, C>
#else
Session::template Deduced<ErrorOr<std::size_t>, C>
#endif
Session::connect(
    C&& completion /**< A callable handler of type `void(ErrorOr<size_t>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!legacyConnectors_.empty(),
                        "Session::connect: No legacy connectors passed "
                        "in Session::create");
    ConnectionWishList wishes;
    for (const auto& c: legacyConnectors_)
        wishes.emplace_back(c);
    return connect(std::move(wishes), std::forward<C>(completion));
}

//------------------------------------------------------------------------------
/** @copydetails Session::connect(C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<std::size_t>, C>
#else
Session::template Deduced<ErrorOr<std::size_t>, C>
#endif
Session::connect(
    ThreadSafe,
    C&& completion /**< A callable handler of type void(ErrorOr<size_t>),
                        or a compatible Boost.Asio completion token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!legacyConnectors_.empty(),
                        "Session::connect: No legacy connectors passed "
                        "in Session::create");
    ConnectionWishList wishes;
    for (const auto& c: legacyConnectors_)
        wishes.emplace_back(c);
    return connect(threadSafe, std::move(wishes), std::forward<C>(completion));
}

//------------------------------------------------------------------------------
struct Session::JoinOp
{
    using ResultValue = SessionInfo;
    Session* self;
    Realm r;

    template <typename F> void operator()(F&& f)
    {
        self->doJoin(std::move(r), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeJoin(threadSafe, std::move(r), std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @return A SessionInfo object with details on the newly established session.
    @param completion A callable handler of type `void(ErrorOr<SessionInfo>)`,
           or a compatible Boost.Asio completion token.
    @post `this->state() == SessionState::establishing` if successful
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting JOIN message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not closed
          during the attempt to join.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchRealm if the realm does not exist.
        - SessionErrc::noSuchRole if one of the client roles is not supported on
          the router.
        - SessionErrc::joinError for other errors reported by the router.
        - Some other `std::error_code` for protocol and transport errors. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<SessionInfo>, C>
#else
Session::template Deduced<ErrorOr<SessionInfo>, C>
#endif
Session::join(
    Realm realm,   /**< Details on the realm to join. */
    C&& completion /**< Callable handler of type `void(ErrorOr<SessionInfo>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    return initiate<JoinOp>(std::forward<C>(completion), std::move(realm));
}

//------------------------------------------------------------------------------
/** @copydetails Session::join(Realm, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<SessionInfo>, C>
#else
Session::template Deduced<ErrorOr<SessionInfo>, C>
#endif
Session::join(
    ThreadSafe,
    Realm realm,   /**< Details on the realm to join. */
    C&& completion /**< Callable handler of type `void(ErrorOr<SessionInfo>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    return safelyInitiate<JoinOp>(std::forward<C>(completion),
                                  std::move(realm));
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
/** @details The "wamp.close.close_realm" reason is sent as part of the
             outgoing `GOODBYE` message.
    @return The _Reason_ URI and details from the `GOODBYE` response returned
            by the router.
    @post `this->state() == SessionState::shuttingDown` if successful
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting GOODBYE message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          while attempting to leave.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer
          before a `GOODBYE` response was received.
        - Some other `std::error_code` for protocol and transport errors. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<Reason>, C>
#endif
Session::leave(
    C&& completion /**< Callable handler of type `void(ErrorOr<Reason>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    return leave(Reason("wamp.close.close_realm"), std::forward<C>(completion));
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
    C&& completion /**< Callable handler of type `void(ErrorOr<Reason>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    return leave(threadSafe, Reason("wamp.close.close_realm"),
                 std::forward<C>(completion));
}

//------------------------------------------------------------------------------
/** @return The _Reason_ URI and details from the `GOODBYE` response returned
            by the router.
    @post `this->state() == SessionState::shuttingDown` if successful
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting GOODBYE message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          during the attempt to leave.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer
          before a `GOODBYE` response was received.
        - Some other `std::error_code` for protocol and transport errors. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Reason>, C>
#else
Session::template Deduced<ErrorOr<Reason>, C>
#endif
Session::leave(
    Reason reason, /**< %Reason URI and other options */
    C&& completion /**< Callable handler of type `void(ErrorOr<Reason>)`,
                        or a compatible Boost.Asio completion token. */
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
    Reason reason, /**< %Reason URI and other options */
    C&& completion /**< Callable handler of type `void(ErrorOr<Reason>)`,
                        or a compatible Boost.Asio completion token. */
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
/** @see @ref Subscriptions

    @return A Subscription object, therafter used to manage the subscription's
            lifetime.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting SUBSCRIBE message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          while attempting to subscribe.
        - SessionErrc::subscribeError if the router replied with an `ERROR`
          response.
        - Some other `std::error_code` for protocol and transport errors. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Subscription>, C>
#else
Session::template Deduced<ErrorOr<Subscription>, C>
#endif
Session::subscribe(
    Topic topic,         /**< The topic to subscribe to. */
    EventSlot eventSlot, /**< Callable handler of type `void (Event)` to execute
                              when a matching event is received. */
    C&& completion       /**< Callable handler of type
                              `void(ErrorOr<Subscription>)`, or a compatible
                              Boost.Asio completion token. */
    )
{
    return initiate<SubscribeOp>(std::forward<C>(completion), std::move(topic),
                                 std::move(eventSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::subscribe(Topic, EventSlot, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Subscription>, C>
#else
Session::template Deduced<ErrorOr<Subscription>, C>
#endif
Session::subscribe(
    ThreadSafe,
    Topic topic,         /**< The topic to subscribe to. */
    EventSlot eventSlot, /**< Callable handler of type `void (Event)` to execute
                              when a matching event is received. */
    C&& completion       /**< Callable handler of type
                             `void(ErrorOr<Subscription>)`, or a compatible
                              Boost.Asio completion token. */
    )
{
    return safelyInitiate<SubscribeOp>(std::forward<C>(completion),
                                       std::move(topic), std::move(eventSlot));
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
/** @details
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
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          while attempting to subsubscribe.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchSubscription if the router reports that there was
          no such subscription.
        - SessionErrc::unsubscribeError if the router reports some other
          error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if the given subscription is empty */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<bool>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::unsubscribe(
    Subscription sub, /**< The subscription to unsubscribe from. */
    C&& completion    /**< Callable handler of type `void(ErrorOr<bool>)`,
                           or a compatible Boost.Asio completion token. */
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
    Subscription sub, /**< The subscription to unsubscribe from. */
    C&& completion    /**< Callable handler of type `void(ErrorOr<bool>)`,
                           or a compatible Boost.Asio completion token. */
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
/** @return The publication ID for this event.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting PUBLISH message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          during the attempt to publish.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::publishError if the router replies with an ERROR
          response.
        - Some other `std::error_code` for protocol and transport errors. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<PublicationId>, C>
#else
Session::template Deduced<ErrorOr<PublicationId>, C>
#endif
Session::publish(
    Pub pub,       /**< The publication to publish. */
    C&& completion /**< Callable handler of type `void(ErrorOr<PublicationId>)`,
                        or a compatible Boost.Asio completion token. */
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
    Pub pub,       /**< The publication to publish. */
    C&& completion /**< Callable handler of type `void(ErrorOr<PublicationId>)`,
                        or a compatible Boost.Asio completion token. */
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

    template <typename F> void operator()(F&& f)
    {
        self->doEnroll(std::move(p), std::move(s), nullptr, std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeEnroll(std::move(p), std::move(s), nullptr,
                         std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @see @ref Registrations

    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting REGISTER message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          during the attempt to enroll.
        - SessionErrc::procedureAlreadyExists if the router reports that the
          procedure has already been registered for this realm.
        - SessionErrc::registerError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
*/
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    Procedure procedure, /**< The procedure to register. */
    CallSlot callSlot,   /**< Callable handler of type `Outcome (Invocation)`
                              to execute when the RPC is invoked. */
    C&& completion       /**< Callable handler of type
                              'void(ErrorOr<Registration>)', or a compatible
                              Boost.Asio completion token. */
)
{
    return initiate<EnrollOp>(
        std::forward<C>(completion), std::move(procedure), std::move(callSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::enroll(Procedure, CallSlot, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    ThreadSafe,
    Procedure procedure, /**< The procedure to register. */
    CallSlot callSlot,   /**< Callable handler of type `Outcome (Invocation)`
                              to execute when the RPC is invoked. */
    C&& completion       /**< Callable handler of type
                              'void(ErrorOr<Registration>)', or a compatible
                              Boost.Asio completion token. */
    )
{
    return safelyInitiate<EnrollOp>(
        std::forward<C>(completion), std::move(procedure), std::move(callSlot));
}

//------------------------------------------------------------------------------
struct Session::EnrollIntrOp
{
    using ResultValue = Registration;
    Session* self;
    Procedure p;
    CallSlot c;
    InterruptSlot i;

    template <typename F> void operator()(F&& f)
    {
        self->doEnroll(std::move(p), std::move(c), std::move(i),
                       std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeEnroll(std::move(p), std::move(c), std::move(i),
                         std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @see @ref Registrations

    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting REGISTER message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          during the attempt to enroll.
        - SessionErrc::procedureAlreadyExists if the router reports that the
          procedure has already been registered for this realm.
        - SessionErrc::registerError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    Procedure procedure, /**< The procedure to register. */
    CallSlot callSlot,   /**< Callable handler of type `Outcome (Invocation)`
                              to execute when the RPC is invoked. */
    InterruptSlot
        interruptSlot,   /**< Callable handler of type `Outcome (Invocation)`
                              to execute when the RPC is interrupted. */
    C&& completion       /**< Callable handler of type
                              `void(ErrorOr<Registration>)`, or a compatible
                              Boost.Asio completion token. */
    )
{
    return initiate<EnrollIntrOp>(
        std::forward<C>(completion), std::move(procedure), std::move(callSlot),
                        std::move(interruptSlot));
}

//------------------------------------------------------------------------------
/** @copydetails Session::enroll(Procedure, CallSlot, InterruptSlot, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Registration>, C>
#else
Session::template Deduced<ErrorOr<Registration>, C>
#endif
Session::enroll(
    ThreadSafe,
    Procedure procedure, /**< The procedure to register. */
    CallSlot callSlot,   /**< Callable handler of type `Outcome (Invocation)`
                              to execute when the RPC is invoked. */
    InterruptSlot
        interruptSlot,   /**< Callable handler of type `Outcome (Invocation)`
                              to execute when the RPC is interrupted. */
    C&& completion       /**< Callable handler of type
                              'void(ErrorOr<Registration>)', or a compatible
                              Boost.Asio completion token. */
    )
{
    return safelyInitiate<EnrollIntrOp>(
        std::forward<C>(completion), std::move(procedure), std::move(callSlot),
        std::move(interruptSlot));
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
/** @details
    If the registration is no longer applicable, then this operation will
    effectively do nothing and a `false` value will be emitted via the
    completion handler.
    @see Registration, ScopedRegistration
    @returns `true` if the registration was found.
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `!!reg == true`
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          while attempting to unregister.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchRegistration if the router reports that there is
          no such procedure registered by that name.
        - SessionErrc::unregisterError if the router reports some other
          error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if the given registration is empty */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<bool>, C>
#else
Session::template Deduced<ErrorOr<bool>, C>
#endif
Session::unregister(
    Registration reg, /**< The RPC registration to unregister. */
    C&& completion    /**< Callable handler of type `void(ErrorOr<bool>)`,
                           or a compatible Boost.Asio completion token. */
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
    Registration reg, /**< The RPC registration to unregister. */
    C&& completion    /**< Callable handler of type `void(ErrorOr<bool>)`,
                           or a compatible Boost.Asio completion token. */
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
    CallChit* c;

    template <typename F> void operator()(F&& f)
    {
        self->doOneShotCall(std::move(r), c, std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeOneShotCall(std::move(r), c, std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @return The remote procedure result.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting CALL message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          during the attempt to call.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchProcedure if the router reports that there is
          no such procedure registered by that name.
        - SessionErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - SessionErrc::callError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @note Use Session::ongoingCall if progressive results are desired.
    @pre `rpc.withProgressiveResults() == false`
    @throws error::Logic if `rpc.progressiveResultsAreEnabled() == true`. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<Result>, C>
#endif
Session::call(
    Rpc rpc,       /**< Details about the RPC. */
    C&& completion /**< Callable handler of type `void(ErrorOr<Result>)`,
                        or a compatible Boost.Asio completion token. */
)
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use Session::ongoingCall for progressive results");
    return initiate<CallOp>(std::forward<C>(completion), std::move(rpc),
                            nullptr);
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
    Rpc rpc,       /**< Details about the RPC. */
    C&& completion /**< Callable handler of type `void(ErrorOr<Result>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use Session::ongoingCall for progressive results");
    return safelyInitiate<CallOp>(std::forward<C>(completion), std::move(rpc),
                                  nullptr);
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
    Rpc rpc,        /**< Details about the RPC. */
    CallChit& chit, /**< [out] Token that can be used to cancel the RPC. */
    C&& completion  /**< Callable handler of type `void(ErrorOr<Result>)`, or
                         a compatible Boost.Asio completion token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use Session::ongoingCall for progressive results");
    return initiate<CallOp>(std::forward<C>(completion), std::move(rpc), &chit);
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
    Rpc rpc,        /**< Details about the RPC. */
    CallChit& chit, /**< [out] Token that can be used to cancel the RPC. */
    C&& completion  /**< Callable handler of type `void(ErrorOr<Result>)`,
                         or a compatible Boost.Asio completion token. */
    )
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use Session::ongoingCall for progressive results");
    return safelyInitiate<CallOp>(std::forward<C>(completion), std::move(rpc),
                                  &chit);
}

//------------------------------------------------------------------------------
struct Session::OngoingCallOp
{
    using ResultValue = Result;
    Session* self;
    Rpc r;
    CallChit* c;

    template <typename F> void operator()(F&& f)
    {
        self->doOngoingCall(std::move(r), c, std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeOngoingCall(std::move(r), c, std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @return The remote procedure result.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting CALL message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          during the attempt to call.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchProcedure if the router reports that there is
          no such procedure registered by that name.
        - SessionErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - SessionErrc::callError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @note `withProgessiveResults(true)` is automatically performed on the
           given `rpc` argument.
    @note The given completion handler must allow multi-shot invocation. */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<Result>, C>
#endif
Session::ongoingCall(
    Rpc rpc,       /**< Details about the RPC. */
    C&& completion /**< Callable handler of type `void(ErrorOr<Result>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    return initiate<OngoingCallOp>(std::forward<C>(completion), std::move(rpc),
                                   nullptr);
}

//------------------------------------------------------------------------------
/** @copydetails Session::ongoingCall(Rpc, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<Result>, C>
#endif
Session::ongoingCall(
    ThreadSafe,
    Rpc rpc,       /**< Details about the RPC. */
    C&& completion /**< Callable handler of type `void(ErrorOr<Result>)`,
                        or a compatible Boost.Asio completion token. */
    )
{
    return safelyInitiate<OngoingCallOp>(std::forward<C>(completion),
                                         std::move(rpc), nullptr);
}

//------------------------------------------------------------------------------
/** @copydetails Session::ongoingCall(Rpc, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<Result>, C>
#endif
Session::ongoingCall(
    Rpc rpc,        /**< Details about the RPC. */
    CallChit& chit, /**< [out] Token that can be used to cancel the RPC. */
    C&& completion  /**< Callable handler of type `void(ErrorOr<Result>)`, or
                         a compatible Boost.Asio completion token. */
    )
{
    return initiate<OngoingCallOp>(std::forward<C>(completion), std::move(rpc),
                                   &chit);
}

//------------------------------------------------------------------------------
/** @copydetails Session::ongoingCall(Rpc, C&&) */
//------------------------------------------------------------------------------
template <typename C>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<Result>, C>
#else
Session::template Deduced<ErrorOr<Result>, C>
#endif
Session::ongoingCall(
    ThreadSafe,
    Rpc rpc,        /**< Details about the RPC. */
    CallChit& chit, /**< [out] Token that can be used to cancel the RPC. */
    C&& completion  /**< Callable handler of type `void(ErrorOr<Result>)`,
                         or a compatible Boost.Asio completion token. */
    )
{
    return safelyInitiate<OngoingCallOp>(std::forward<C>(completion),
                                         std::move(rpc), &chit);
}

//------------------------------------------------------------------------------
template <typename O, typename C, typename... As>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<typename O::ResultValue>, C>
#else
Session::template Deduced<ErrorOr<typename O::ResultValue>, C>
#endif
Session::initiate(C&& token, As&&... args)
{
    return boost::asio::async_initiate<
        C, void(ErrorOr<typename O::ResultValue>)>(
        O{this, std::forward<As>(args)...}, token);
}

//------------------------------------------------------------------------------
template <typename O, typename C, typename... As>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<typename O::ResultValue>, C>
#else
Session::template Deduced<ErrorOr<typename O::ResultValue>, C>
#endif
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
