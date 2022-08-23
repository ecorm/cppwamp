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
#include "peerdata.hpp"
#include "registration.hpp"
#include "subscription.hpp"
#include "tagtypes.hpp"
#include "wampdefs.hpp"
#include "internal/clientinterface.hpp"

namespace wamp
{

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
    // TODO: Remove heap allocation requirement

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
    static Ptr create(const AnyIoExecutor& exec);

    /** Creates a new Session instance. */
    static Ptr create(const AnyIoExecutor& exec, AnyIoExecutor userExec);

    template <typename TExecutionContext>
    static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<TExecutionContext>())
    create(
        TExecutionContext& context
            /**< Context providing the executor from which Session will extract
                 a strand for its internal I/O operations. */
    )
    {
        return create(context.get_executor());
    }

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead. */
    static Ptr create(AnyIoExecutor userExec, LegacyConnector connector);

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead. */
    static Ptr create(AnyIoExecutor userExec, ConnectorList connectors);

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead.
        @copydetails Session::create(AnyIoExecutor, LegacyConnector)
        @details Only participates in overload resolution when
                 `isExecutionContext<TExecutionContext>() == true`
        @tparam TExecutionContext Must meet the requirements of
                                  Boost.Asio's ExecutionContext */
    template <typename TExecutionContext>
    static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<TExecutionContext>())
    create(
        TExecutionContext& userContext, /**< Provides executor with which to
                                             post all user-provided handlers. */
        LegacyConnector connector /**< Connection details for the
                                       transport to use. */
        )
    {
        return create(userContext.get_executor(), std::move(connector));
    }

    /** Creates a new Session instance.
        @deprecated Pass connection wish list to Session::connect instead.
        @copydetails Session::create(AnyIoExecutor, ConnectorList)
        @details Only participates in overload resolution when
                 `isExecutionContext<TExecutionContext>() == true`
        @tparam TExecutionContext Must meet the requirements of
                                  Boost.Asio's ExecutionContext */
    template <typename TExecutionContext>
    static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<TExecutionContext>())
    create(
        TExecutionContext& userContext, /**< Provides executor with which to
                                             post all user-provided handlers. */
        ConnectorList connectors  /**< Connection details for the
                                       transport to use. */
    )
    {
        return create(userContext.get_executor(), std::move(connectors));
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

    /** Obtains the execution context in which which I/O operations are
        serialized. */
    const IoStrand& strand() const;

    /** Obtains the fallback executor used to execute user-provided handlers. */
    AnyIoExecutor userExecutor() const;

    /** Legacy function kept for backward compatiblity. */
    CPPWAMP_DEPRECATED AnyIoExecutor userIosvc() const;

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
    // TODO: Rename to terminate
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

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    initiate(C&& token, As&&... args);

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    safelyInitiate(C&& token, As&&... args);

    CPPWAMP_HIDDEN explicit Session(const AnyIoExecutor& exec,
                                    AnyIoExecutor userExec);

    CPPWAMP_HIDDEN explicit Session(AnyIoExecutor userExec,
                                    ConnectorList connectors);

    IoStrand strand_;
    ConnectorList legacyConnectors_;
    ImplPtr impl_;

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
    ConnectionWishList wishes;

    template <typename F> void operator()(F&& f)
    {
        self->impl_->connect(std::move(wishes), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeConnect(std::move(wishes), std::forward<F>(f));
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
    Realm realm;

    template <typename F> void operator()(F&& f)
    {
        self->impl_->join(std::move(realm), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->join(threadSafe, std::move(realm), std::forward<F>(f));
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
        self->impl_->leave(std::move(reason), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeLeave(std::move(reason), std::forward<F>(f));
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
        self->impl_->subscribe(move(topic), move(slot), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        using std::move;
        self->impl_->safeSubscribe(move(topic), move(slot), std::forward<F>(f));
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
        self->impl_->unsubscribe(std::move(sub), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeUnsubscribe(std::move(sub), std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @details
    This function may be called during any session state. If the subscription
    is no longer applicable, then the unsubscribe operation will effectively
    do nothing and a `false` value will be emitted via the completion handler.
    If there are other local subscriptions on this session remaining for the
    same topic, then the session does not send an `UNSUBSCRIBE` message to
    the router.
    @see Subscription, ScopedSubscription
    @returns `false` if the subscription was already removed, `true` otherwise.
    @note Duplicate unsubscribes using the same Subscription handle
          are safely ignored.
    @pre `!!sub == true`
    @par Error Codes
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
        self->impl_->publish(std::move(pub), std::move(std::forward<F>(f)));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safePublish(std::move(pub), std::move(std::forward<F>(f)));
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
        self->impl_->enroll(std::move(procedure), std::move(slot), nullptr,
                            std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeEnroll(std::move(procedure), std::move(slot), nullptr,
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
        self->impl_->enroll(std::move(procedure), std::move(callSlot),
                            std::move(interruptSlot), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeEnroll(std::move(procedure), std::move(callSlot),
                                std::move(interruptSlot), std::forward<F>(f));
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
        self->impl_->unregister(std::move(reg), std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeUnregister(std::move(reg), std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @details
    This function may be called during any session state. If the subscription
    is no longer applicable, then the unregister operation will effectively
    do nothing and a `false` value will be emitted via the completion handler.
    @see Registration, ScopedRegistration
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
    CallChit* chit;

    template <typename F> void operator()(F&& f)
    {
        self->impl_->oneShotCall(std::move(rpc), chit, std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeOneShotCall(std::move(rpc), chit, std::forward<F>(f));
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
    Rpc rpc;
    CallChit* chit;

    template <typename F> void operator()(F&& f)
    {
        self->impl_->ongoingCall(std::move(rpc), chit, std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->impl_->safeOngoingCall(std::move(rpc), chit, std::forward<F>(f));
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
