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
    if (impl_)
        impl_->safeDisconnect();
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
CPPWAMP_INLINE SessionState Session::state() const
{
    return impl_->state();
}

//------------------------------------------------------------------------------
/** @details
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
CPPWAMP_INLINE void Session::setLogHandler(
    LogHandler handler /**< Callable handler of type `<void (LogEntry)>`. */
)
{
    impl_->setLogHandler(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setLogHandler(LogHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setLogHandler(
    ThreadSafe,
    LogHandler handler /**< Callable handler of type `<void (LogEntry)>`. */
)
{
    impl_->safeSetLogHandler(handler);
}

//------------------------------------------------------------------------------
/** @details
    The default log level is LogLevel::warning if never set.
    @note This method is thread-safe.
    @see Session::setLogHandler */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setLogLevel(LogLevel level)
{
    impl_->setLogLevel(level);
}

//------------------------------------------------------------------------------
/** @details
    Copies of the handler are made when they are dispatched. If the handler
    needs to be stateful, or is non-copyable, then pass a stateless copyable
    proxy instead.

    @note No state change events are fired when the session object is
          terminating. */
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
/** @returns `true` if the authentication was sent, a std::error_code otherwise.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting AUTHENTICATE message
          exceeds the transport's limits.
        - SessionErrc::invalidState if the session was not authenticating
          during the attempt to authenticate (can be safely discarded). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone Session::authenticate(
    Authentication auth /**< Contains the authentication signature
                             and other options. */
)
{
    return impl_->authenticate(std::move(auth));
}

//------------------------------------------------------------------------------
/** @copydetails Session::authenticate(Authentication)
    @note It is safe to call `get()` on the returned `std::future` within the
          same thread as the one used by Session::strand. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone> Session::authenticate(
    ThreadSafe,
    Authentication auth /**< Contains the authentication signature
                             and other options. */
)
{
    return impl_->safeAuthenticate(std::move(auth));
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
/** @details
    This function can be safely called during any session state. If the
    subscription is no longer applicable, then this operation will
    effectively do nothing.
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
/** @returns `true` if the authentication was sent, a std::error_code otherwise.
    @par Error Codes
        - SessionErrc::payloadSizeExceeded if the resulting PUBLISH message exceeds
          the transport's limits.
        - SessionErrc::invalidState if the session was not established
          during the attempt to publish (can be safely discarded). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone Session::publish(
    Pub pub /**< The publication to publish. */
)
{
    return impl_->publish(std::move(pub));
}

//------------------------------------------------------------------------------
/** @copydetails Session::publish(Pub pub)
    @note It is safe to call `get()` on the returned `std::future` within the
          same thread as the one used by Session::strand. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone> Session::publish(
    ThreadSafe,
    Pub pub /**< The publication to publish. */
)
{
    return impl_->safePublish(std::move(pub));
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any session state. If the
    registration is no longer applicable, then this operation will
    effectively do nothing.
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
/** @returns `true` or `false` depending if a pending call matching the given
              chit was found.
    @par Error Codes
        - SessionErrc::invalidState if the session was not established
          during the attempt to cancel (can be safely discarded). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone Session::cancel(
    CallChit chit /**< Contains the request ID of the call to cancel. */
    )
{
    return cancel(chit, chit.cancelMode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit)
    @note It is safe to call `get()` on the returned `std::future` within the
          same thread as the one used by Session::strand. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone> Session::cancel(
    ThreadSafe,
    CallChit chit /**< Contains the request ID of the call to cancel. */
    )
{
    return cancel(threadSafe, chit, chit.cancelMode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone Session::cancel(
    CallChit chit,      /**< Contains the request ID of the call to cancel. */
    CallCancelMode mode /**< The mode with which to cancel the call. */
    )
{
    return impl_->cancelCall(chit.requestId(), mode);
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(ThreadSafe, CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone> Session::cancel(
    ThreadSafe,
    CallChit chit,      /**< Contains the request ID of the call to cancel. */
    CallCancelMode mode /**< The mode with which to cancel the call. */
    )
{
    return impl_->safeCancelCall(chit.requestId(), mode);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::doConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {impl_->connect(std::move(w), std::move(f));}

CPPWAMP_INLINE void Session::safeConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {impl_->safeConnect(std::move(w), std::move(f));}

CPPWAMP_INLINE void Session::doJoin(Realm&& r, ChallengeHandler c, CompletionHandler<SessionInfo>&& f)
    {impl_->join(std::move(r), std::move(c), std::move(f));}

CPPWAMP_INLINE void Session::safeJoin(Realm&& r, ChallengeHandler c, CompletionHandler<SessionInfo>&& f)
    {impl_->safeJoin(std::move(r), std::move(c), std::move(f));}

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
