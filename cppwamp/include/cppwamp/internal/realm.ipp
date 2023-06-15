/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../realm.hpp"
#include "routerrealm.hpp"

namespace wamp
{

CPPWAMP_INLINE Reason Realm::defaultKillReason()
{
    return Reason{WampErrc::sessionKilled};
}

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
Realm::getSession(SessionId sid) const
{
    auto s = impl_->getSession(sid);
    if (!s)
        return makeUnexpectedError(WampErrc::noSuchSession);
    return s;
}

CPPWAMP_INLINE ErrorOr<bool> Realm::killSessionById(SessionId sid, Reason r)
{
    return impl_->killSessionById(sid, std::move(r));
}

CPPWAMP_INLINE Realm::SessionIdSet
Realm::killSessionIf(const SessionPredicate& filter, Reason r)
{
    return impl_->killSessionIf(std::move(filter), std::move(r));
}

CPPWAMP_INLINE Realm::SessionIdSet Realm::killSessions(SessionIdSet set,
                                                       Reason r)
{
    return impl_->killSessions(std::move(set), std::move(r));
}

CPPWAMP_INLINE ErrorOr<RegistrationInfo>
Realm::getRegistration(RegistrationId rid, bool listCallees) const
{
    return impl_->getRegistration(rid, listCallees);
}

CPPWAMP_INLINE ErrorOr<RegistrationInfo>
Realm::lookupRegistration(const Uri& uri, MatchPolicy p,
                          bool listCallees) const
{
    return impl_->lookupRegistration(uri, p, listCallees);
}

CPPWAMP_INLINE ErrorOr<RegistrationInfo>
Realm::bestRegistrationMatch(const Uri& uri, bool listCallees) const
{
    return impl_->bestRegistrationMatch(uri, listCallees);
}

CPPWAMP_INLINE std::size_t
Realm::forEachRegistration(MatchPolicy p, const RegistrationPredicate& f) const
{
    return impl_->forEachRegistration(p, f);
}

CPPWAMP_INLINE ErrorOr<SubscriptionInfo>
Realm::getSubscription(SubscriptionId sid, bool listSubscribers) const
{
    return impl_->getSubscription(sid, listSubscribers);
}

CPPWAMP_INLINE ErrorOr<SubscriptionInfo>
Realm::lookupSubscription(const Uri& uri, MatchPolicy p,
                          bool listSubscribers) const
{
    return impl_->lookupSubscription(uri, p, listSubscribers);
}

CPPWAMP_INLINE std::size_t
Realm::forEachSubscription(MatchPolicy p, const SubscriptionPredicate& f) const
{
    return impl_->forEachSubscription(p, f);
}

CPPWAMP_INLINE std::size_t Realm::forEachMatchingSubscription(
    const Uri& uri, const SubscriptionPredicate& f) const
{
    return impl_->forEachMatchingSubscription(uri, f);
}

CPPWAMP_INLINE Realm::Realm(std::shared_ptr<internal::RouterRealm> impl,
                            FallbackExecutor fe)
    : fallbackExecutor_(std::move(fe)),
      impl_(std::move(impl))
{}

} // namespace wamp
