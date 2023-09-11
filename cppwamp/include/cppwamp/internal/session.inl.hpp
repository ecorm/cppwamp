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
    Executor exec, /**< Executor from which Session will extract
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
// NOLINTNEXTLINE(bugprone-exception-escape)
CPPWAMP_INLINE Session::~Session()
{
    if (impl_)
        impl_->disconnect();
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
    return fallbackExecutor_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionState Session::state() const
{
    return impl_->state();
}

//------------------------------------------------------------------------------
/** @details
    Message tracing is disabled by default.
    @see Session::observeIncidents */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::enableTracing(bool enabled)
{
    impl_->enableTracing(enabled);
}

//------------------------------------------------------------------------------
/** @details
    The fallback timeout period is indefinite, by default.
    @throws error::Logic if the given timeout period is negative. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setFallbackTimeout(Timeout timeout)
{
    impl_->setFallbackTimeout(internal::checkTimeout(timeout));
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
/** @details
    Equivalent to Subscription::unsubscribe.
    @throws error::Logic if the subscription is active and not owned
            by the Session
    @see Subscription, ScopedSubscription */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unsubscribe(
    Subscription sub /**< The subscription to unsubscribe from. */
)
{
    CPPWAMP_LOGIC_CHECK(canUnsubscribe(sub),
                        "Session does not own the subscription");
    sub.unsubscribe();
}

//------------------------------------------------------------------------------
/** @details Does nothing if the session is not established. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::publish(
    Pub pub /**< The publication to publish. */
)
{
    impl_->publish(std::move(pub));
}

//------------------------------------------------------------------------------
/** @details
    Equivalent to Registration::unregister.
    @see Registration, ScopedRegistration */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unregister(
    Registration reg /**< The RPC registration to unregister. */
)
{
    CPPWAMP_LOGIC_CHECK(canUnregister(reg),
                        "Session does not own the registration");
    reg.unregister();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(std::shared_ptr<internal::Peer> peer,
                                Executor exec)
    : fallbackExecutor_(exec),
      impl_(internal::Client::create(std::move(peer), std::move(exec)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(std::shared_ptr<internal::Peer> peer,
                                Executor exec, FallbackExecutor fallbackExec)
    : fallbackExecutor_(std::move(fallbackExec)),
      impl_(internal::Client::create(std::move(peer), std::move(exec)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::directConnect(any link)
{
    impl_->directConnect(std::move(link));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Session::canUnsubscribe(const Subscription& sub) const
{
    return sub.canUnsubscribe({}, *impl_);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Session::canUnregister(const Registration& reg) const
{
    return reg.canUnregister({}, *impl_);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setIncidentHandler(IncidentSlot&& s)
{
    impl_->observeIncidents(std::move(s));
}

CPPWAMP_INLINE void Session::doConnect(ConnectionWishList&& w,
                                       CompletionHandler<size_t>&& f)
{
    impl_->connect(std::move(w), std::move(f));
}

CPPWAMP_INLINE void Session::doJoin(Petition&& p, ChallengeSlot&& s,
                                    CompletionHandler<Welcome>&& f)
{
    impl_->join(std::move(p), std::move(s), std::move(f));
}

CPPWAMP_INLINE void Session::doLeave(Reason&& r, Timeout t,
                                     CompletionHandler<Reason>&& f)
{
    impl_->leave(std::move(r), t, std::move(f));
}

CPPWAMP_INLINE void Session::doDisconnect(Timeout t,
                                          CompletionHandler<bool>&& f)
{
    impl_->disconnect(t, std::move(f));
}

CPPWAMP_INLINE void Session::doSubscribe(Topic&& t, EventSlot&& s,
                                         CompletionHandler<Subscription>&& f)
{
    impl_->subscribe(std::move(t), std::move(s), std::move(f));
}

CPPWAMP_INLINE void Session::doUnsubscribe(
    const Subscription& s, Timeout t, CompletionHandler<bool>&& f)
{
    impl_->unsubscribe(s, t, std::move(f));
}

CPPWAMP_INLINE void Session::doPublish(
    Pub&& p, CompletionHandler<PublicationId>&& f)
{
    impl_->publish(std::move(p), std::move(f));
}

CPPWAMP_INLINE void Session::doEnroll(
    Procedure&& p, CallSlot&& c, InterruptSlot&& i,
    CompletionHandler<Registration>&& f)
{
    impl_->enroll(std::move(p), std::move(c), std::move(i), std::move(f));
}

CPPWAMP_INLINE void Session::doUnregister(
    const Registration& r, Timeout t, CompletionHandler<bool>&& f)
{
    impl_->unregister(r, t, std::move(f));
}

CPPWAMP_INLINE void Session::doCall(Rpc&& r, CompletionHandler<Result>&& f)
{
    impl_->call(std::move(r), std::move(f));
}

CPPWAMP_INLINE void Session::doEnroll(Stream&& s, StreamSlot&& ss,
                                      CompletionHandler<Registration>&& f)
{
    impl_->enroll(std::move(s), std::move(ss), std::move(f));
}

CPPWAMP_INLINE void Session::doRequestStream(
    StreamRequest&& r, CallerChunkSlot&& c,
    CompletionHandler<CallerChannel>&& f)
{
    impl_->requestStream(std::move(r), std::move(c), std::move(f));
}

CPPWAMP_INLINE void Session::doOpenStream(
    StreamRequest&& r, CallerChunkSlot&& s,
    CompletionHandler<CallerChannel>&& f)
{
    impl_->openStream(std::move(r), std::move(s), std::move(f));
}

} // namespace wamp
