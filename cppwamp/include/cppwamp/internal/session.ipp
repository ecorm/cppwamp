/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../session.hpp"
#include "../api.hpp"
#include "client.hpp"
#include "networkpeer.hpp"

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
    : Session(std::make_shared<internal::NetworkPeer>(false), std::move(exec))
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
    : Session(std::make_shared<internal::NetworkPeer>(false), std::move(exec),
              std::move(fallbackExec))
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
CPPWAMP_INLINE const IoStrand& Session::strand() const
{
    return impl_->strand();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const Session::Executor& Session::executor() const
{
    return impl_->executor();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const Session::FallbackExecutor&
Session::fallbackExecutor() const
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
    Message tracing is disabled by default.
    @note This method is thread-safe.
    @see Session::observeIncidents */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::enableTracing(bool enabled)
{
    impl_->enableTracing(enabled);
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
    @note The log, challenge, and state change handlers will *not*
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
/** @returns `true` if the authentication was sent, a std::error_code
    otherwise. */
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
CPPWAMP_INLINE Session::Session(std::shared_ptr<internal::Peer> peer,
                                Executor exec)
    : fallbackExecutor_(exec),
      impl_(internal::Client::create(std::move(peer), std::move(exec)))
{}

CPPWAMP_INLINE Session::Session(std::shared_ptr<internal::Peer> peer,
                                const Executor& exec,
                                FallbackExecutor fallbackExec)
    : fallbackExecutor_(fallbackExec),
      impl_(internal::Client::create(std::move(peer), exec,
                                   std::move(fallbackExec)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::directConnect(any link)
{
    impl_->directConnect(std::move(link));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setIncidentHandler(IncidentSlot&& s)
    {impl_->observeIncidents(std::move(s));}

CPPWAMP_INLINE void Session::safeSetIncidentHandler(IncidentSlot&& s)
    {impl_->safeObserveIncidents(std::move(s));}

CPPWAMP_INLINE void Session::doConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {impl_->connect(std::move(w), std::move(f));}

CPPWAMP_INLINE void Session::safeConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {impl_->safeConnect(std::move(w), std::move(f));}

CPPWAMP_INLINE void Session::doJoin(Realm&& r, ChallengeSlot&& c, CompletionHandler<Welcome>&& f)
    {impl_->join(std::move(r), std::move(c), std::move(f));}

CPPWAMP_INLINE void Session::safeJoin(Realm&& r, ChallengeSlot&& c, CompletionHandler<Welcome>&& f)
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

CPPWAMP_INLINE void Session::doEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i, CompletionHandler<Registration>&& f)
    {impl_->enroll(std::move(p), std::move(c), std::move(i), std::move(f));}

CPPWAMP_INLINE void Session::safeEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i, CompletionHandler<Registration>&& f)
    {impl_->safeEnroll(std::move(p), std::move(c), std::move(i), std::move(f));}

CPPWAMP_INLINE void Session::doUnregister(const Registration& r, CompletionHandler<bool>&& f)
    {impl_->unregister(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::safeUnregister(const Registration& r, CompletionHandler<bool>&& f)
    {impl_->safeUnregister(r, std::move(f));}

CPPWAMP_INLINE void Session::doCall(Rpc&& r, CompletionHandler<Result>&& f)
    {impl_->call(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::safeCall(Rpc&& r, CompletionHandler<Result>&& f)
    {impl_->safeCall(std::move(r), std::move(f));}

CPPWAMP_INLINE void Session::doEnroll(Stream&& s, StreamSlot&& ss, CompletionHandler<Registration>&& f)
    {impl_->enroll(std::move(s), std::move(ss), std::move(f));}

CPPWAMP_INLINE void Session::safeEnroll(Stream&& s, StreamSlot&& ss, CompletionHandler<Registration>&& f)
    {impl_->safeEnroll(std::move(s), std::move(ss), std::move(f));}

CPPWAMP_INLINE void Session::doRequestStream(StreamRequest&& r, CallerChunkSlot&& c, CompletionHandler<CallerChannel>&& f)
    {impl_->requestStream(std::move(r), std::move(c), std::move(f));}

CPPWAMP_INLINE void Session::safeRequestStream(StreamRequest&& r, CallerChunkSlot&& c, CompletionHandler<CallerChannel>&& f)
    {impl_->safeRequestStream(std::move(r), std::move(c), std::move(f));}

CPPWAMP_INLINE ErrorOr<CallerChannel> Session::doOpenStream(StreamRequest&& r, CallerChunkSlot&& s)
    {return impl_->openStream(std::move(r), std::move(s));}

CPPWAMP_INLINE std::future<ErrorOr<CallerChannel>> Session::safeOpenStream(StreamRequest&& r, CallerChunkSlot&& s)
    {return impl_->safeOpenStream(std::move(r), std::move(s));}

} // namespace wamp
