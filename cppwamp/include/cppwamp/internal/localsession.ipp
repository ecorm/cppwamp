/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../localsession.hpp"
#include "../api.hpp"
#include "localsessionimpl.hpp"

namespace wamp
{

CPPWAMP_INLINE LocalSession::LocalSession() {}

CPPWAMP_INLINE bool LocalSession::expired() const
{
    return !impl_ || impl_->expired();
}

CPPWAMP_INLINE Subscription LocalSession::subscribe(Topic topic,
                                                    EventSlot eventSlot)
{
    return impl_->subscribe(std::move(topic), std::move(eventSlot));
}

CPPWAMP_INLINE std::future<Subscription>
LocalSession::subscribe(ThreadSafe, Topic topic, EventSlot eventSlot)
{
    return impl_->safeSubscribe(std::move(topic), std::move(eventSlot));
}

CPPWAMP_INLINE void LocalSession::unsubscribe(Subscription sub)
{
    impl_->unsubscribe(sub);
}

CPPWAMP_INLINE void LocalSession::unsubscribe(ThreadSafe, Subscription sub)
{
    impl_->safeUnsubscribe(sub);
}

CPPWAMP_INLINE PublicationId LocalSession::publish(Pub pub)
{
    return impl_->publish(std::move(pub));
}

CPPWAMP_INLINE std::future<PublicationId> LocalSession::publish(ThreadSafe,
                                                                Pub pub)
{
    return impl_->safePublish(std::move(pub));
}

CPPWAMP_INLINE Registration LocalSession::enroll(Procedure procedure,
                                                 CallSlot callSlot)
{
    return impl_->enroll(std::move(procedure), std::move(callSlot), nullptr);
}

CPPWAMP_INLINE std::future<Registration>
LocalSession::enroll(ThreadSafe, Procedure procedure, CallSlot callSlot)
{
    return impl_->safeEnroll(std::move(procedure), std::move(callSlot),
                             nullptr);
}

CPPWAMP_INLINE Registration
LocalSession::enroll(Procedure procedure, CallSlot callSlot,
                     InterruptSlot interruptSlot)
{
    return impl_->enroll(std::move(procedure), std::move(callSlot),
                         std::move(interruptSlot));
}

CPPWAMP_INLINE std::future<Registration>
LocalSession::enroll(ThreadSafe, Procedure procedure, CallSlot callSlot,
                     InterruptSlot interruptSlot)
{
    return impl_->safeEnroll(std::move(procedure), std::move(callSlot),
                             std::move(interruptSlot));
}

CPPWAMP_INLINE void LocalSession::unregister(Registration reg)
{
    impl_->unregister(reg);
}

CPPWAMP_INLINE void LocalSession::unregister(ThreadSafe, Registration reg)
{
    impl_->safeUnregister(reg);
}

CPPWAMP_INLINE bool LocalSession::cancel(CallChit chit)
{
    return impl_->cancelCall(chit.requestId(), chit.cancelMode()).value();
}

CPPWAMP_INLINE std::future<bool> LocalSession::cancel(ThreadSafe, CallChit chit)
{
    return cancel(threadSafe, chit, chit.cancelMode());
}

CPPWAMP_INLINE bool LocalSession::cancel(CallChit chit, CallCancelMode mode)
{
    return impl_->cancelCall(chit.requestId(), mode).value();
}

CPPWAMP_INLINE std::future<bool> LocalSession::cancel(ThreadSafe, CallChit chit,
                                                      CallCancelMode mode)
{
    struct Dispatched
    {
        ImplPtr impl;
        RequestId r;
        CallCancelMode m;
        std::promise<bool> p;

        void operator()()
        {
            try
            {
                p.set_value(impl->cancelCall(r, m).value());
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<bool> p;
    auto fut = p.get_future();
    boost::asio::dispatch(impl_->strand(),
                          Dispatched{impl_, chit.requestId(), mode});
    return fut;
}

CPPWAMP_INLINE LocalSession::LocalSession(ImplPtr impl)
    : impl_(std::move(impl))
{}

CPPWAMP_INLINE void LocalSession::doOneShotCall(Rpc&& r, CallChit* c,
                                                CompletionHandler<Result>&& f)
{
    impl_->oneShotCall(std::move(r), c, std::move(f));
}

CPPWAMP_INLINE void LocalSession::safeOneShotCall(Rpc&& r, CallChit* c,
                                                  CompletionHandler<Result>&& f)
{
    impl_->safeOneShotCall(std::move(r), c, std::move(f));
}

CPPWAMP_INLINE void LocalSession::doOngoingCall(Rpc&& r, CallChit* c,
                                                OngoingCallHandler&& f)
{
    impl_->ongoingCall(std::move(r), c, std::move(f));
}

CPPWAMP_INLINE void LocalSession::safeOngoingCall(Rpc&& r, CallChit* c,
                                                  OngoingCallHandler&& f)
{
    impl_->safeOngoingCall(std::move(r), c, std::move(f));
}

} // namespace wamp
