/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../realm.hpp"
#include "routerrealm.hpp"

namespace wamp
{

const Realm::Executor& Realm::executor() const {return impl_->executor();}

const IoStrand& Realm::strand() const {return impl_->strand();}

const Uri& Realm::uri() const {return impl_->uri();}

void Realm::observe(RealmObserver::Ptr o) {impl_->observe(std::move(o));}

void Realm::unobserve() {impl_->unobserve();}

void Realm::doCountSessions(CompletionHandler<std::size_t> h)
{
    impl_->countSessions(std::move(h));
}

void Realm::doListSessions(CompletionHandler<std::vector<SessionId>> h)
{
    impl_->listSessions(std::move(h));
}

void Realm::doForEachSession(SessionHandler f, CompletionHandler<std::size_t> h)
{
    impl_->forEachSession(std::move(f), std::move(h));
}

void Realm::doLookupSession(SessionId sid,
                            CompletionHandler<ErrorOr<SessionDetails>> h)
{
    impl_->lookupSession(sid, std::move(h));
}

void Realm::doKillSession(SessionId sid, CompletionHandler<bool> h)
{
    impl_->killSession(sid, std::move(h));
}

void Realm::doKillSessions(SessionFilter f, CompletionHandler<std::size_t> h)
{
    impl_->killSessions(std::move(f), std::move(h));
}

void Realm::doListRegistrations(CompletionHandler<RegistrationLists> h)
{
    impl_->listRegistrations(std::move(h));
}

void Realm::doForEachRegistration(MatchPolicy p, RegistrationHandler f,
                                  CompletionHandler<std::size_t> h)
{
    impl_->forEachRegistration(p, std::move(f), std::move(h));
}

void Realm::doLookupRegistration(
    Uri uri, MatchPolicy p, CompletionHandler<ErrorOr<RegistrationDetails>> h)
{
    impl_->lookupRegistration(std::move(uri), p, std::move(h));
}

void Realm::doMatchRegistration(
    Uri uri, CompletionHandler<ErrorOr<RegistrationDetails>> h)
{
    impl_->matchRegistration(std::move(uri), std::move(h));
}

void Realm::doGetRegistration(RegistrationId rid,
                              CompletionHandler<ErrorOr<RegistrationDetails>> h)
{
    impl_->getRegistration(rid, std::move(h));
}

void Realm::doListSubscriptions(CompletionHandler<SubscriptionLists> h)
{
    impl_->listSubscriptions(std::move(h));
}

void Realm::doForEachSubscription(MatchPolicy p, SubscriptionHandler f,
                                  CompletionHandler<std::size_t> h)
{
    impl_->forEachSubscription(p, std::move(f), std::move(h));
}

void Realm::doLookupSubscription(
    Uri uri, MatchPolicy p, CompletionHandler<ErrorOr<SubscriptionDetails>> h)
{
    impl_->lookupSubscription(std::move(uri), p, std::move(h));
}

void Realm::doMatchSubscriptions(
    Uri uri, CompletionHandler<std::vector<SubscriptionId>> h)
{
    impl_->matchSubscriptions(std::move(uri), std::move(h));
}

void Realm::doGetSubscription(
    SubscriptionId sid, CompletionHandler<ErrorOr<SubscriptionDetails>> h)
{
    impl_->getSubscription(sid, std::move(h));
}

} // namespace wamp
