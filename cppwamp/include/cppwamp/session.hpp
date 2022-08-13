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

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "chits.hpp"
#include "config.hpp"
#include "connector.hpp"
#include "error.hpp"
#include "erroror.hpp"
#include "peerdata.hpp"
#include "registration.hpp"
#include "subscription.hpp"
#include "traits.hpp"
#include "wampdefs.hpp"
#include "internal/clientinterface.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type used to specify than an operation is to be dispatched via the
    called objects's execution strand.
    Use the @ref threadSafe constant to conveniently pass this tag
    to functions. */
//------------------------------------------------------------------------------
struct ThreadSafe
{
    constexpr ThreadSafe() = default;
};

//------------------------------------------------------------------------------
/** Constant ThreadSafe object instance that can be passed to functions. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE_VARIABLE constexpr ThreadSafe threadSafe;


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

    The `boost::asio::io_context` passed via `create()` is used when executing
    handler functions passed-in by the user. This can be the same, or different
    than the `io_context` passed to the `Connector` creation functions.

    @par Aborting Asynchronous Operations
    All pending asynchronous operations can be _aborted_ by dropping the client
    connection via Session::disconnect. Pending post-join operations can be also
    be aborted via Session::leave. There is currently no way to abort a single
    operation without dropping the connection or leaving the realm.

    @par Terminating Asynchronous Operations
    All pending asynchronous operations can be _terminated_ by dropping the
    client connection via Session::reset or the Session destructor. By design,
    the handlers for pending operations will not be invoked if they
    were terminated in this way (this will result in hung coroutine operations).
    This is useful if a client application needs to shutdown abruptly and cannot
    enforce the lifetime of objects accessed within the asynchronous operation
    handlers.

    @par Thread-safety
    Undecorated methods must be called within the Session's execution
    [strand](https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/strands.html).
    If the same `io_context` is used by a single-threaded app and a Session's
    transport, calls to undecorated methods will implicitly be within the
    Session's strand. If invoked from other threads, calls to undecorated
    methods must be executed via
    `boost::asio::dispatch(session->stand(), operation)`. Session methods
    decorated with the ThreadSafe tag type may be safely used concurrently by
    multiple threads. These decorated methods take care of executing operations
    via a Session's strand so that they become sequential.

    @see ErrorOr, Registration, Subscription. */
//------------------------------------------------------------------------------
class CPPWAMP_API Session : public std::enable_shared_from_this<Session>
{
private:
    struct GenericOp { template <typename F> void operator()(F&&) {} };

public:
    /** Shared pointer to a Session. */
    using Ptr = std::shared_ptr<Session>;

    /** Enumerates the possible states that a Session can be in. */
    using State = SessionState;

    /** Type-erased wrapper around a WAMP event handler. */
    using EventSlot = AnyReusableHandler<void (Event)>;

    /** Type-erased wrapper around an RPC handler. */
    using CallSlot = AnyReusableHandler<Outcome (Invocation)>;

    /** Type-erased wrapper around an RPC interruption handler. */
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;

    /** Type-erased wrapper around a log event handler. */
    using LogHandler = AnyReusableHandler<void(std::string)>;

    /** Type-erased wrapper around a Session state change handler. */
    using StateChangeHandler = AnyReusableHandler<void(SessionState)>;

    /** Type-erased wrapper around an authentication challenge handler. */
    using ChallengeHandler = AnyReusableHandler<void(Challenge)>;

    /** Obtains the type returned by [boost::asio::async_initiate]
        (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/async_initiate.html)
        with given the completion token type `C` and signature `void(T)`.

        Token Type                   | Deduced Return Type
        ---------------------------- | -------------------
        Callback function            | `void`
        `boost::asio::yield_context` | `ErrorOr<Value>`
        `boost::asio::use_awaitable` | An awaitable yielding `ErrorOr<Value>`
        `boost::asio::use_future`    | `std::future<ErrorOr<Value>>` */
    template <typename T, typename C>
    using Deduced = decltype(
        boost::asio::async_initiate<C, void(T)>(std::declval<GenericOp&>(),
                                                std::declval<C&>()));
    /** Creates a new Session instance. */
    static Ptr create(AnyIoExecutor exec, const Connector::Ptr& connector);

    /** Creates a new Session instance. */
    static Ptr create(AnyIoExecutor exec, const ConnectorList& connectors);

    /** Creates a new Session instance.
        @copydetails Session::create(AnyIoExecutor, const Connector::Ptr&)
        @details Only participates in overload resolution when
                 `isExecutionContext<TExecutionContext>() == true`
        @tparam TExecutionContext Must meet the requirements of
                                  Boost.Asio's ExecutionContext */
    template <typename TExecutionContext>
    static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<TExecutionContext>())
    create(
        TExecutionContext& context, /**< Provides executor with which to
                                         post all user-provided handlers. */
        const Connector::Ptr& connector /**< Connection details for the
                                             transport to use. */
        )
    {
        return create(context.get_executor(), connector);
    }

    /** Creates a new Session instance.
        @copydetails Session::create(AnyIoExecutor, const ConnectorList&)
        @details Only participates in overload resolution when
                 `isExecutionContext<TExecutionContext>() == true`
        @tparam TExecutionContext Must meet the requirements of
                                  Boost.Asio's ExecutionContext */
    template <typename TExecutionContext>
    static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<TExecutionContext>())
    create(
        TExecutionContext& context, /**< Provides executor with which to
                                         post all user-provided handlers. */
        const ConnectorList& connectors  /**< Connection details for the
                                              transport to use. */
    )
    {
        return create(context.get_executor(), connectors);
    }

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

    /** Obtains the fallback executor used to execute user-provided handlers. */
    AnyIoExecutor userExecutor() const;

    /** Legacy function kept for backward compatiblity. */
    CPPWAMP_DEPRECATED AnyIoExecutor userIosvc() const;

    /** Obtains the execution context in which which I/O operations are
        serialized. */
    IoStrand strand() const;

    /** Returns the current state of the session. */
    SessionState state() const;
    /// @}

    /// @name Modifiers
    /// @{
    /** Sets the log handler that is dispatched for warnings. */
    void setWarningHandler(LogHandler handler);

    /** Thread-safe setting of warning handler. */
    void setWarningHandler(ThreadSafe, LogHandler handler);

    /** Sets the log handler that is dispatched for debug traces. */
    void setTraceHandler(LogHandler handler);

    /** Thread-safe setting of trace handler. */
    void setTraceHandler(ThreadSafe, LogHandler handler);

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
    connect(C&& completion);

    /** Thread-safe connect. */
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
    void authenticate(Authentication auth);

    /** Thread-safe authenticate. */
    void authenticate(ThreadSafe, Authentication auth);

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
    void reset();

    /** Thread-safe reset. */
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
    void publish(Pub pub);

    /** Thread-safe publish. */
    void publish(ThreadSafe, Pub pub);

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

    /** Cancels a remote procedure using the cancel mode that was specified
        in the @ref wamp::Rpc "Rpc". */
    void cancel(CallChit);

    /** Thread-safe cancel. */
    void cancel(ThreadSafe, CallChit);

    /** Cancels a remote procedure using the given mode. */
    void cancel(CallChit, CallCancelMode mode);

    /** Thread-safe cancel with a given mode. */
    void cancel(ThreadSafe, CallChit, CallCancelMode mode);

    /** Cancels a remote procedure.
        @deprecated Use the overload taking a CallChit. */
    void cancel(CallCancellation cancellation);

    /** Thread-safe cancel.
        @deprecated Use the overload taking a CallChit. */
    void cancel(ThreadSafe, CallCancellation cancellation);
    /// @}

private:
    using ImplPtr = typename internal::ClientInterface::Ptr;

    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<T>)>;

    template <typename T>
    using ReusableHandler = AnyReusableHandler<void(T)>;

    using OneShotCallHandler   = CompletionHandler<Result>;
    using MultiShotCallHandler = AnyReusableHandler<void(ErrorOr<Result>)>;

    // These initiator function objects are needed due to C++11 lacking
    // generic lambdas.
    template <typename O> class SafeOp;
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

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    initiate(C&& token, As&&... args);

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    safelyInitiate(C&& token, As&&... args);

    template <typename F>
    void dispatchViaStrand(F&& operation);

    template <typename TResultValue, typename F>
    bool checkState(State expectedState, F& handler);

    void asyncConnect(CompletionHandler<size_t>&& handler);

    CPPWAMP_HIDDEN explicit Session(AnyIoExecutor userExec,
                                    const ConnectorList& connectors);

    CPPWAMP_HIDDEN void warn(std::string message);

    CPPWAMP_HIDDEN void setState(SessionState state);

    CPPWAMP_HIDDEN void doConnect(
        size_t index, std::shared_ptr<CompletionHandler<size_t>> handler);

    CPPWAMP_INLINE void onConnectFailure(
        size_t index, std::error_code ec,
        std::shared_ptr<CompletionHandler<size_t>> handler);

    CPPWAMP_INLINE void onConnectSuccess(
        size_t index, ImplPtr impl,
            std::shared_ptr<CompletionHandler<size_t>> handler);

    AnyIoExecutor userExecutor_;
    ConnectorList connectors_;
    Connector::Ptr currentConnector_;
    ReusableHandler<std::string> warningHandler_;
    ReusableHandler<std::string> traceHandler_;
    ReusableHandler<State> stateChangeHandler_;
    ReusableHandler<Challenge> challengeHandler_;
    std::atomic<SessionState> state_;
    bool isTerminating_ = false;
    ImplPtr impl_;

    // TODO: Remove this once CoroSession is removed
    template <typename> friend class CoroSession;
};


//******************************************************************************
// Session template function implementations
//******************************************************************************

//------------------------------------------------------------------------------
template <typename O>
class Session::SafeOp
{
public:
    using ResultValue = typename O::ResultValue;

    template <typename... Ts>
    SafeOp(Session* self, Ts&&... args)
        : self_(self),
          operation_({self, std::forward<Ts>(args)...})
    {}

    template <typename F>
    void operator()(F&& handler)
    {
        struct Dispatched
        {
            Session::Ptr self;
            O operation;
            typename std::decay<F>::type handler;

            void operator()()
            {
                operation(std::move(handler));
            }
        };

        boost::asio::dispatch(self_->strand(),
                              Dispatched{self_->shared_from_this(),
                                         std::move(operation_),
                                         std::forward<F>(handler)});
    }

private:
    Session* self_;
    O operation_;
};

//------------------------------------------------------------------------------
struct Session::ConnectOp
{
    using ResultValue = size_t;
    Session* self;
    template <typename F> void operator()(F&& f)
    {
        self->asyncConnect(std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @details
    The session will attempt to connect using the transports that were
    specified by the wamp::Connector objects passed during create().
    If more than one transport was specified, they will be traversed in the
    same order as they appeared in the @ref ConnectorList.
    @return The index of the Connector object used to establish the connetion.
    @post `this->state() == SessionState::connecting` if successful
    @par Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - SessionErrc::allTransportsFailed if more than one transport was
          specified and they all failed to connect.
        - SessionErrc::invalidState if the session was not disconnected
          before the attempt to connect.
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
    return initiate<ConnectOp>(std::forward<C>(completion));
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
    return safelyInitiate<ConnectOp>(std::forward<C>(completion));
}

//------------------------------------------------------------------------------
struct Session::JoinOp
{
    using ResultValue = SessionInfo;
    Session* self;
    Realm realm;
    template <typename F> void operator()(F&& f)
    {
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::closed, handler))
            self->impl_->join(std::move(realm), std::move(handler));
    }
};

//------------------------------------------------------------------------------
/** @return A SessionInfo object with details on the newly established session.
    @param completion A callable handler of type `void(ErrorOr<SessionInfo>)`,
           or a compatible Boost.Asio completion token.
    @post `this->state() == SessionState::establishing` if successful
    @par Error Codes
        - SessionErrc::invalidState if the session was not closed
          before the attempt to join.
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
    Reason reason;
    template <typename F> void operator()(F&& f)
    {
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
            self->impl_->leave(std::move(reason), std::move(handler));
    }
};

//------------------------------------------------------------------------------
/** @details The "wamp.close.close_realm" reason is sent as part of the
             outgoing `GOODBYE` message.
    @return The _Reason_ URI and details from the `GOODBYE` response returned
            by the router.
    @post `this->state() == SessionState::shuttingDown` if successful
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to leave.
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
        - SessionErrc::invalidState if the session was not established
          before the attempt to leave.
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
    Topic topic;
    EventSlot slot;
    template <typename F> void operator()(F&& f)
    {
        using std::move;
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
            self->impl_->subscribe(move(topic), move(slot), move(handler));
    }
};

//------------------------------------------------------------------------------
/** @see @ref Subscriptions

    @return A Subscription object, therafter used to manage the subscription's
            lifetime.
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to subscribe.
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
    Subscription sub;
    template <typename F> void operator()(F&& f)
    {
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
            self->impl_->unsubscribe(std::move(sub), std::move(handler));
    }
};

//------------------------------------------------------------------------------
/** @details
    If there are other local subscriptions on this session remaining for the
    same topic, then the session does not send an `UNSUBSCRIBE` message to
    the router.
    @see Subscription, ScopedSubscription
    @returns `false` if the subscription was already removed, `true` otherwise.
    @note Duplicate unsubscribes using the same Subscription handle
          are safely ignored.
    @pre `!!sub == true`
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to unsubscribe.
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
    Pub pub;
    template <typename F> void operator()(F&& f)
    {
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
            self->impl_->publish(std::move(pub), std::move(handler));
    }
};

//------------------------------------------------------------------------------
/** @return The publication ID for this event.
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to publish.
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
    Procedure procedure;
    CallSlot slot;
    template <typename F> void operator()(F&& f)
    {
        using std::move;
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
            self->impl_->enroll(move(procedure), move(slot), nullptr,
                                move(handler));
    }
};

//------------------------------------------------------------------------------
/** @see @ref Registrations

    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to enroll.
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
    Procedure procedure;
    CallSlot callSlot;
    InterruptSlot interruptSlot;
    template <typename F> void operator()(F&& f)
    {
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
        {
            self->impl_->enroll(std::move(procedure), std::move(callSlot),
                                std::move(interruptSlot), std::move(handler));
        }
    }
};

//------------------------------------------------------------------------------
/** @see @ref Registrations

    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to enroll.
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
    Registration reg;
    template <typename F> void operator()(F&& f)
    {
        CompletionHandler<ResultValue> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
            self->impl_->unregister(std::move(reg), std::move(handler));
    }
};

//------------------------------------------------------------------------------
/** @see Registration, ScopedRegistration
    @returns `false` if the registration was already removed, `true` otherwise.
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `!!reg == true`
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to unregister.
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
    Rpc rpc;
    CallChit* chitPtr;

    template <typename F>
    void operator()(F&& f)
    {
        if (chitPtr)
            *chitPtr = {};
        auto chit = call(std::is_copy_constructible<ValueTypeOf<F>>{},
                         std::forward<F>(f));
        if (chitPtr)
            *chitPtr = chit;
    }

    template <typename F>
    CallChit call(std::true_type, F&& f)
    {
        using std::move;
        CallChit chit;
        if (rpc.progressiveResultsAreEnabled())
        {
            MultiShotCallHandler handler(std::forward<F>(f));
            if (self->checkState<Result>(State::established, handler))
                chit = self->impl_->ongoingCall(move(rpc), move(handler));
        }
        else
        {
            OneShotCallHandler handler(std::forward<F>(f));
            if (self->checkState<Result>(State::established, handler))
                chit = self->impl_->oneShotCall(move(rpc), move(handler));
        }
        return chit;
    }

    template <typename F>
    CallChit call(std::false_type, F&& f)
    {
        using std::move;
        CallChit chit;
        CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                            "Progressive results require copyable "
                            "multi-shot handler");
        CompletionHandler<Result> handler(std::forward<F>(f));
        if (self->checkState<ResultValue>(State::established, handler))
            chit = self->impl_->oneShotCall(move(rpc), move(handler));
        return chit;
    }
};

//------------------------------------------------------------------------------
/** @return The remote procedure result.
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          before the attempt to call.
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchProcedure if the router reports that there is
          no such procedure registered by that name.
        - SessionErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - SessionErrc::callError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @note If progressive results are enabled, then the given (or generated)
          completion handler must be copy-constructible so that it can be
          executed multiple times (checked at runtime).
    @pre `rpc.withProgressiveResults() == false ||
          std::is_copy_constructible_v<std::remove_cvref_t<C>>`
    @throws error::Logic if progressive results are enabled but the given
            (or generated) completion handler is not copy-constructible. */
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
    return safelyInitiate<CallOp>(std::forward<C>(completion), std::move(rpc),
                                  &chit);
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

    return initiate<SafeOp<O>>(std::forward<C>(token),
                               std::forward<As>(args)...);
}

//------------------------------------------------------------------------------
template <typename F>
void Session::dispatchViaStrand(F&& operation)
{
    boost::asio::dispatch(strand(), std::forward<F>(operation));
}

//------------------------------------------------------------------------------
template <typename TResultValue, typename F>
bool Session::checkState(State expectedState, F& handler)
{
    bool valid = state() == expectedState;
    if (!valid)
    {
        ErrorOr<TResultValue> e{makeUnexpectedError(SessionErrc::invalidState)};
        postVia(userExecutor_, std::move(handler), std::move(e));
    }
    return valid;
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/session.ipp"
#endif

#endif // CPPWAMP_SESSION_HPP
