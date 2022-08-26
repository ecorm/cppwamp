/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../session.hpp"
#include "../api.hpp"
#include "client.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** @copydetails Session(Executor) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    Executor exec /**< Executor used for internal I/O operations and as
                       a fallback for user-provided handlers. */
)
{
    return Ptr(new Session(std::move(exec)));
}

//------------------------------------------------------------------------------
/** @copydetails Session(const Executor&, FallbackExecutor) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    const Executor& exec,         /**< Executor from which Session will extract
                                       a strand for its internal I/O operations. */
    FallbackExecutor fallbackExec /**< Fallback executor to use for
                                       user-provided handlers. */
)
{
    return Ptr(new Session(exec, std::move(fallbackExec)));
}

//------------------------------------------------------------------------------
/** @details
    The provided executor serves as a fallback when asynchronous operation
    handlers don't bind a specific executor (in lieu of using the system
    executor as fallback.
    From the given connector(s), session will extract an execution strand for
    use with its internal I/O operations.
    @post `this->state() == SessionState::disconnected`
    @post `this->fallbackExecutor() == exec`
    @return A shared pointer to the created session object. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    FallbackExecutor fallbackExec, /**< Fallback executor for
                                        user-provided handlers. */
    LegacyConnector connector      /**< Connection details for the transport
                                        to use. */
)
{
    return Ptr(new Session(std::move(fallbackExec),
                           ConnectorList{std::move(connector)}));
}

//------------------------------------------------------------------------------
/** @copydetails Session::create(FallbackExecutor, LegacyConnector)
    @pre `connectors.empty() == false`
    @post `this->state() == SessionState::disconnected`
    @return A shared pointer to the created Session object.
    @throws error::Logic if `connectors.empty() == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    FallbackExecutor fallbackExec, /**< Fallback executor with which to execute
                                        user-provided handlers. */
    ConnectorList connectors       /**< A list of connection details for
                                        the transports to use. */
)
{
    CPPWAMP_LOGIC_CHECK(!connectors.empty(), "Connector list is empty");
    return Ptr(new Session(std::move(fallbackExec), std::move(connectors)));
}

//------------------------------------------------------------------------------
/** @details
    Session will extract a strand from the given executor for use with its
    internal I/O operations. The given executor also serves as fallback
    for user-provided handlers.
    @post `this->fallbackExecutor() == exec` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(
    Executor exec /**< Executor for internal I/O operations, as well as
                       fallback for user-provided handlers. */
)
    : impl_(internal::Client::create(std::move(exec)))
{}

//------------------------------------------------------------------------------
/** @details
    @post `this->fallbackExecutor() == exec` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(
    const Executor& exec, /**< Executor from which Session will extract
                               a strand for its internal I/O operations */
    FallbackExecutor fallbackExec /**< Fallback executor to use for
                                       user-provided handlers. */
)
    : impl_(internal::Client::create(exec, std::move(fallbackExec)))
{}

//------------------------------------------------------------------------------
/** @details
    Automatically invokes disconnect() on the session, which drops the
    connection and cancels all pending asynchronous operations. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::~Session()
{
    // CoroSession does not define a destructor, so ~Session does not need
    // to be virtual.
    if (impl_)
    {
        impl_->safeDisconnect();
        impl_.reset();
    }
}

//------------------------------------------------------------------------------
/** @details
    The dictionary is structured as per `HELLO.Details.roles`, as desribed in
    the ["Client: Role and Feature Announcement"][1] section of the advanced
    WAMP specification.

    [1]: https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.7.1.1.1 */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const Object& Session::roles()
{
    return internal::Client::roles();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const IoStrand& Session::strand() const
{
    return impl_->strand();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::FallbackExecutor Session::fallbackExecutor() const
{
    return impl_->userExecutor();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::FallbackExecutor Session::userExecutor() const
{
    return impl_->userExecutor();
}

//------------------------------------------------------------------------------
/** @deprecated Use wamp::Session::fallbackExecutor instead. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::FallbackExecutor Session::userIosvc() const
{
    return fallbackExecutor();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionState Session::state() const
{
    return impl_->state();
}

//------------------------------------------------------------------------------
/** @details
    Warnings occur when the session encounters problems that do not prevent
    it from proceeding normally. An example of such warnings is when a
    peer attempts to send an event with arguments that does not match the types
    of a statically-typed event slot.

    By default, warnings are discarded. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setWarningHandler(
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    impl_->setWarningHandler(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setWarningHandler(LogHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setWarningHandler(
    ThreadSafe,
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    impl_->safeSetWarningHandler(handler);
}

//------------------------------------------------------------------------------
/** @details
    By default, debug traces are discarded. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setTraceHandler(
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    impl_->setTraceHandler(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setTraceHandler(LogHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setTraceHandler(
    ThreadSafe,
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    impl_->safeSetTraceHandler(handler);
}

//------------------------------------------------------------------------------
/** @note No state change events are fired when the session object is
          destructing. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setStateChangeHandler(
    StateChangeHandler handler /**< Callable handler of type `<void (SessionState)>`. */
)
{
    impl_->setStateChangeHandler(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setStateChangeHandler(StateChangeHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setStateChangeHandler(
    ThreadSafe,
    StateChangeHandler handler /**< Callable handler of type `<void (SessionState)>`. */
)
{
    impl_->safeSetStateChangeHandler(handler);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setChallengeHandler(
    ChallengeHandler handler /**< Callable handler of type `<void (Challenge)>`. */
)
{
    impl_->setChallengeHandler(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setChallengeHandler(ChallengeHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setChallengeHandler(
    ThreadSafe,
    ChallengeHandler handler /**< Callable handler of type `<void (Challenge)>`. */
    )
{
    impl_->safeSetChallengeHandler(handler);
}

//------------------------------------------------------------------------------
/** @details
    If `this->state() != SessionState::authenticating`, then the authentication
    is discarded and not sent.  */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::authenticate(
    Authentication auth /**< Contains the authentication signature
                             and other options. */
)
{
    impl_->authenticate(std::move(auth));
}

//------------------------------------------------------------------------------
/** @copydetails Session::authenticate(Authentication) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::authenticate(
    ThreadSafe,
    Authentication auth /**< Contains the authentication signature
                             and other options. */
)
{
    impl_->safeAuthenticate(std::move(auth));
}

//------------------------------------------------------------------------------
/** @details
    Aborts all pending asynchronous operations, invoking their handlers
    with error codes indicating that cancellation has occured.
    @post `this->state() == SessionState::disconnected` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::disconnect()
{
    impl_->disconnect();
}

//------------------------------------------------------------------------------
/** @copydetails Session::disconnect */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::disconnect(ThreadSafe)
{
    impl_->safeDisconnect();
}

//------------------------------------------------------------------------------
/** @details
    Terminates all pending asynchronous operations, which does **not**
    invoke their handlers. This is useful when a client application needs
    to shutdown abruptly and cannot enforce the lifetime of objects
    accessed within the asynchronous operation handlers.
    @note The warning, trace, challenge, and state change handlers will *not*
          be fired again until the commencement of the next connect operation.
    @post `this->state() == SessionState::disconnected` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::terminate()
{
    impl_->terminate();
}

//------------------------------------------------------------------------------
/** @copydetails Session::terminate */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::terminate(ThreadSafe)
{
    impl_->safeTerminate();
}

//------------------------------------------------------------------------------
/** @copydetails Session::terminate */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::reset()
{
    impl_->terminate();
}

//------------------------------------------------------------------------------
/** @copydetails Session::terminate */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::reset(ThreadSafe)
{
    impl_->safeTerminate();
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any session state. If the
    subscription is no longer applicable, then the unsubscribe operation
    will effectively do nothing.
    @see Subscription, ScopedSubscription
    @note Duplicate unsubscribes using the same Subscription object
          are safely ignored.
    @pre `bool(sub) == true`
    @throws error::Logic if the given subscription is empty */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unsubscribe(
    Subscription sub /**< The subscription to unsubscribe from. */
)
{
    CPPWAMP_LOGIC_CHECK(bool(sub), "The subscription is empty");
    impl_->unsubscribe(sub);
}

//------------------------------------------------------------------------------
/** @copydetails Session::unsubscribe(Subscription) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unsubscribe(
    ThreadSafe,
    Subscription sub /**< The subscription to unsubscribe from. */
    )
{
    CPPWAMP_LOGIC_CHECK(bool(sub), "The subscription is empty");
    impl_->safeUnsubscribe(sub);
}

//------------------------------------------------------------------------------
/** @details
    If `this->state() != SessionState::established`, then the publication is
    discarded and not sent. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::publish(
    Pub pub /**< The publication to publish. */
)
{
    impl_->publish(std::move(pub));
}

//------------------------------------------------------------------------------
/** @copydetails Session::publish(Pub pub) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::publish(
    ThreadSafe,
    Pub pub /**< The publication to publish. */
)
{
    impl_->safePublish(std::move(pub));
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any session state. If the
    registration is no longer applicable, then the unregister operation
    will effectively do nothing.
    @see Registration, ScopedRegistration
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `bool(reg) == true`
    @throws error::Logic if the given registration is empty */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unregister(
    Registration reg /**< The RPC registration to unregister. */
)
{
    CPPWAMP_LOGIC_CHECK(bool(reg), "The registration is empty");
    impl_->unregister(reg);
}

//------------------------------------------------------------------------------
/** @copydetails Session::unregister(Registration) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unregister(
    ThreadSafe,
    Registration reg /**< The RPC registration to unregister. */
    )
{
    CPPWAMP_LOGIC_CHECK(bool(reg), "The registration is empty");
    impl_->safeUnregister(reg);
}

//------------------------------------------------------------------------------
/** @details
    If `this->state() != SessionState::established`, then the cancellation
    is discarded and not sent. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    CallChit chit /**< Contains the request ID of the call to cancel. */
    )
{
    return cancel(chit, chit.cancelMode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    ThreadSafe,
    CallChit chit /**< Contains the request ID of the call to cancel. */
    )
{
    return cancel(threadSafe, chit, chit.cancelMode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    CallChit chit,      /**< Contains the request ID of the call to cancel. */
    CallCancelMode mode /**< The mode with which to cancel the call. */
    )
{
    return impl_->cancelCall(chit.requestId(), mode);
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    ThreadSafe,
    CallChit chit,      /**< Contains the request ID of the call to cancel. */
    CallCancelMode mode /**< The mode with which to cancel the call. */
    )
{
    return impl_->safeCancelCall(chit.requestId(), mode);
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    CallCancellation cancellation /**< Contains the request ID
                                       and other options. */
)
{
    return impl_->cancelCall(cancellation.requestId(), cancellation.mode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit, CallCancelMode) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    ThreadSafe,
    CallCancellation cancellation  /**< Contains the request ID
                                         and other options. */
)
{
    return impl_->safeCancelCall(cancellation.requestId(), cancellation.mode());
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(FallbackExecutor fallbackExec,
                                ConnectorList connectors)
    : legacyConnectors_(std::move(connectors)),
      impl_(internal::Client::create(
                boost::asio::make_strand(legacyConnectors_.at(0).executor()),
                std::move(fallbackExec)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::doConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {impl_->connect(std::move(w), std::move(f));}

CPPWAMP_INLINE void Session::safeConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {impl_->safeConnect(std::move(w), std::move(f));}

CPPWAMP_INLINE void Session::doJoin(Realm&& r, CompletionHandler<SessionInfo>&& f)
    {impl_->join(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::safeJoin(Realm&& r, CompletionHandler<SessionInfo>&& f)
    {impl_->safeJoin(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::doLeave(Reason&& r, CompletionHandler<Reason>&& f)
    {impl_->leave(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::safeLeave(Reason&& r, CompletionHandler<Reason>&& f)
    {impl_->safeLeave(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::doSubscribe(Topic&& t, EventSlot&& s,
                          CompletionHandler<Subscription>&& f)
    {impl_->subscribe(std::move(t), std::move(s), std::move(f));}

CPPWAMP_INLINE void Session::safeSubscribe(Topic&& t, EventSlot&& s,
                            CompletionHandler<Subscription>&& f)
    {impl_->safeSubscribe(std::move(t), std::move(s), std::move(f));}

CPPWAMP_INLINE void Session::doUnsubscribe(const Subscription& s, CompletionHandler<bool>&& f)
    {impl_->unsubscribe(std::move(s), std::move(f));}

CPPWAMP_INLINE void Session::safeUnsubscribe(const Subscription& s, CompletionHandler<bool>&& f)
    {impl_->safeUnsubscribe(std::move(s), std::move(f));}

CPPWAMP_INLINE void Session::doPublish(Pub&& p, CompletionHandler<PublicationId>&& f)
    {impl_->publish(std::move(p), std::move(f));}

CPPWAMP_INLINE void Session::safePublish(Pub&& p, CompletionHandler<PublicationId>&& f)
    {impl_->safePublish(std::move(p), std::move(f));}

CPPWAMP_INLINE void Session::doEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                       CompletionHandler<Registration>&& f)
    {impl_->enroll(std::move(p), std::move(c), std::move(i), std::move(f));}

CPPWAMP_INLINE void Session::safeEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                         CompletionHandler<Registration>&& f)
    {impl_->safeEnroll(std::move(p), std::move(c), std::move(i), std::move(f));}

CPPWAMP_INLINE void Session::doUnregister(const Registration& r, CompletionHandler<bool>&& f)
    {impl_->unregister(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::safeUnregister(const Registration& r, CompletionHandler<bool>&& f)
    {impl_->safeUnregister(r, std::move(f));}

CPPWAMP_INLINE void Session::doOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f)
    {impl_->oneShotCall(std::move(r), c, std::move(f));}

CPPWAMP_INLINE void Session::safeOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f)
    {impl_->safeOneShotCall(std::move(r), c, std::move(f));}

CPPWAMP_INLINE void Session::doOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f)
    {impl_->ongoingCall(std::move(r), c, std::move(f));}

CPPWAMP_INLINE void Session::safeOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f)
    {impl_->safeOngoingCall(std::move(r), c, std::move(f));}

} // namespace wamp
