/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../realmobserver.hpp"
#include "../api.hpp"
#include "matchpolicyoption.hpp"
#include "timeformatting.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
SessionDetails::SessionDetails() {};

SessionDetails::SessionDetails(ClientFeatures f, AuthInfo::Ptr a, SessionId i)
    : features(f), authInfo(std::move(a)), id(i)
{}

CPPWAMP_INLINE Object toObject(const SessionDetails& details)
{
    const auto& authInfo = *(details.authInfo);
    return Object
    {
        {"authid",       authInfo.id()},
        {"authmethod",   authInfo.method()},
        {"authprovider", authInfo.provider()},
        {"authrole",     authInfo.role()},
        {"session",      details.id}
        // TODO: transport
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Object toObject(const RegistrationDetails& r)
{
    return Object
    {
        {"created", internal::toRfc3339TimestampInMilliseconds(r.created)},
        {"id",      r.id},
        {"invoke",  "single"}, // TODO: Shared registrations
        {"match",   "exact"},  // TODO: Pattern-based registrations
        {"uri",     r.uri},
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Object toObject(const RegistrationLists& lists)
{
    return Object
    {
        {"exact", lists.exact},
        {"prefix", lists.prefix},
        {"wildcard", lists.exact},
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Object toObject(const SubscriptionDetails& s)
{
    return Object
    {
        {"created", internal::toRfc3339TimestampInMilliseconds(s.created)},
        {"id",      s.id},
        {"match",   internal::toString(s.matchPolicy)},
        {"uri",     s.uri},
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Object toObject(const SubscriptionLists& lists)
{
    return Object
    {
        {"exact", lists.exact},
        {"prefix", lists.prefix},
        {"wildcard", lists.exact},
    };
}

//------------------------------------------------------------------------------
RealmObserver::~RealmObserver() {}

void RealmObserver::onRealmClosed(const Uri&) {}

void RealmObserver::onJoin(const SessionDetails&) {}

void RealmObserver::onLeave(const SessionDetails&) {}

void RealmObserver::onRegister(const SessionDetails&,
                               const RegistrationDetails&) {}

void RealmObserver::onUnregister(const SessionDetails&,
                                 const RegistrationDetails&) {}

void RealmObserver::onSubscribe(const SessionDetails&,
                                const SubscriptionDetails&) {}

void RealmObserver::onUnsubscribe(const SessionDetails&,
                                  const SubscriptionDetails&) {}

} // namespace wamp
