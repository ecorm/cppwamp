/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../realm.hpp"
#include "routerrealm.hpp"

namespace wamp
{

CPPWAMP_INLINE Realm::Realm() {}

CPPWAMP_INLINE Realm::operator bool() const {return bool(impl_);}

CPPWAMP_INLINE const Realm::Executor& Realm::executor() const
{
    return impl_->executor();
}

CPPWAMP_INLINE const IoStrand& Realm::strand() const {return impl_->strand();}

CPPWAMP_INLINE const Uri& Realm::uri() const {return impl_->uri();}

CPPWAMP_INLINE bool Realm::isOpen() const {return impl_->isOpen();}

CPPWAMP_INLINE void Realm::observe(RealmObserver::Ptr o)
{
    impl_->observe(std::move(o));
}

CPPWAMP_INLINE void Realm::unobserve() {impl_->unobserve();}

CPPWAMP_INLINE Realm::Realm(std::shared_ptr<internal::RouterRealm> impl)
    : impl_(std::move(impl))
{}

CPPWAMP_INLINE void Realm::doCountSessions(CompletionHandler<std::size_t> h)
{
    impl_->countSessions(std::move(h));
}

CPPWAMP_INLINE void Realm::doListSessions(
    CompletionHandler<std::vector<SessionId>> h)
{
    impl_->listSessions(std::move(h));
}

CPPWAMP_INLINE void Realm::doForEachSession(SessionHandler f,
                                            CompletionHandler<std::size_t> h)
{
    impl_->forEachSession(std::move(f), std::move(h));
}

CPPWAMP_INLINE void Realm::doLookupSession(
    SessionId sid, CompletionHandler<ErrorOr<SessionDetails>> h)
{
    impl_->lookupSession(sid, std::move(h));
}

CPPWAMP_INLINE void Realm::doKillSession(SessionId sid,
                                         CompletionHandler<bool> h)
{
    impl_->killSession(sid, std::move(h));
}

CPPWAMP_INLINE void Realm::doKillSessions(SessionFilter f,
                                          CompletionHandler<std::size_t> h)
{
    impl_->killSessions(std::move(f), std::move(h));
}

CPPWAMP_INLINE void Realm::doListRegistrations(
    CompletionHandler<RegistrationLists> h)
{
    impl_->listRegistrations(std::move(h));
}

CPPWAMP_INLINE void Realm::doForEachRegistration(
    MatchPolicy p, RegistrationHandler f, CompletionHandler<std::size_t> h)
{
    impl_->forEachRegistration(p, std::move(f), std::move(h));
}

CPPWAMP_INLINE void Realm::doLookupRegistration(
    Uri uri, MatchPolicy p, CompletionHandler<ErrorOr<RegistrationDetails>> h)
{
    impl_->lookupRegistration(std::move(uri), p, std::move(h));
}

CPPWAMP_INLINE void Realm::doMatchRegistration(
    Uri uri, CompletionHandler<ErrorOr<RegistrationDetails>> h)
{
    impl_->matchRegistration(std::move(uri), std::move(h));
}

CPPWAMP_INLINE void Realm::doGetRegistration(
    RegistrationId rid, CompletionHandler<ErrorOr<RegistrationDetails>> h)
{
    impl_->getRegistration(rid, std::move(h));
}

CPPWAMP_INLINE void Realm::doListSubscriptions(
    CompletionHandler<SubscriptionLists> h)
{
    impl_->listSubscriptions(std::move(h));
}

CPPWAMP_INLINE void Realm::doForEachSubscription(
    MatchPolicy p, SubscriptionHandler f, CompletionHandler<std::size_t> h)
{
    impl_->forEachSubscription(p, std::move(f), std::move(h));
}

CPPWAMP_INLINE void Realm::doLookupSubscription(
    Uri uri, MatchPolicy p, CompletionHandler<ErrorOr<SubscriptionDetails>> h)
{
    impl_->lookupSubscription(std::move(uri), p, std::move(h));
}

CPPWAMP_INLINE void Realm::doMatchSubscriptions(
    Uri uri, CompletionHandler<std::vector<SubscriptionId>> h)
{
    impl_->matchSubscriptions(std::move(uri), std::move(h));
}

CPPWAMP_INLINE void Realm::doGetSubscription(
    SubscriptionId sid, CompletionHandler<ErrorOr<SubscriptionDetails>> h)
{
    impl_->getSubscription(sid, std::move(h));
}

} // namespace wamp
