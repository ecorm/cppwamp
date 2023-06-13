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

CPPWAMP_INLINE Realm::operator bool() const {return isAttached();}

CPPWAMP_INLINE const Realm::Executor& Realm::executor() const
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return impl_->executor();
}

CPPWAMP_INLINE const Realm::FallbackExecutor& Realm::fallbackExecutor() const
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return fallbackExecutor_;
}

CPPWAMP_INLINE const IoStrand& Realm::strand() const
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return impl_->strand();
}

CPPWAMP_INLINE const Uri& Realm::uri() const
{
    static const std::string empty;
    return isAttached() ? impl_->uri() : empty;
}

CPPWAMP_INLINE bool Realm::isAttached() const {return bool(impl_);}

CPPWAMP_INLINE bool Realm::isOpen() const
{
    return isAttached() && impl_->isOpen();
}

CPPWAMP_INLINE void Realm::observe(RealmObserver::Ptr o)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    impl_->observe(std::move(o), fallbackExecutor_);
}

CPPWAMP_INLINE std::size_t Realm::sessionCount() const
{
    return impl_->sessionCount();
}

CPPWAMP_INLINE std::size_t
Realm::forEachSession(const SessionPredicate& handler) const
{
    if (!isAttached())
        return 0;
    return impl_->forEachSession(handler);
}

CPPWAMP_INLINE ErrorOr<SessionInfo::ConstPtr>
Realm::lookupSession(SessionId sid) const
{
    auto s = impl_->lookupSession(sid);
    if (!s)
        return makeUnexpectedError(WampErrc::noSuchSession);
    return s;
}

CPPWAMP_INLINE Realm::Realm(std::shared_ptr<internal::RouterRealm> impl,
                            FallbackExecutor fe)
    : fallbackExecutor_(std::move(fe)),
      impl_(std::move(impl))
{}

CPPWAMP_INLINE void Realm::doKillSessionById(SessionId sid, Reason r,
                                             CompletionHandler<ErrorOr<bool>> h)
{
    impl_->killSessionById(sid, std::move(r), std::move(h));
}

CPPWAMP_INLINE void Realm::doKillSessions(SessionPredicate f, Reason r,
                                          CompletionHandler<SessionIdList> h)
{
    impl_->killSessions(std::move(f), std::move(r), std::move(h));
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
    Uri uri, CompletionHandler<SubscriptionIdList> h)
{
    impl_->matchSubscriptions(std::move(uri), std::move(h));
}

CPPWAMP_INLINE void Realm::doGetSubscription(
    SubscriptionId sid, CompletionHandler<ErrorOr<SubscriptionDetails>> h)
{
    impl_->getSubscription(sid, std::move(h));
}

} // namespace wamp
