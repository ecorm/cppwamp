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
CPPWAMP_INLINE SessionDetails::SessionDetails() {}

CPPWAMP_INLINE SessionDetails::SessionDetails(ClientFeatures f, AuthInfo::Ptr a)
    : features(f),
      authInfo(std::move(a))
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
        {"session",      authInfo.sessionId()}
        // TODO: transport
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionJoinInfo::SessionJoinInfo() {}

CPPWAMP_INLINE void convert(FromVariantConverter& conv, SessionJoinInfo& s)
{
    conv("authid",       s.authid,       "")
        ("authmethod",   s.authmethod,   "")
        ("authprovider", s.authprovider, "")
        ("authrole",     s.authrole,     "")
        ("session",      s.sessionId,    0)
        ("transport",    s.transport,    Object{});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionLeftInfo::SessionLeftInfo() {}

CPPWAMP_INLINE SessionLeftInfo parseSessionLeftInfo(const Event& event)
{
    SessionLeftInfo s;
    s.sessionId = 0;
    event.convertTo(s.sessionId, s.authid, s.authrole);
    return s;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE RegistrationInfo::RegistrationInfo() {}

CPPWAMP_INLINE RegistrationInfo::RegistrationInfo(
    Uri uri, TimePoint created, RegistrationId id, MatchPolicy mp,
    InvocationPolicy ip)
    : uri(std::move(uri)), created(created), id(id), matchPolicy(mp),
      invocationPolicy(ip)
{}

CPPWAMP_INLINE void convert(FromVariantConverter& conv,
                            RegistrationInfo& r)
    {
        String created;
        Variant match;
        String invoke;

        conv("created", created, "")
            ("id",      r.id,    0)
            ("invoke",  invoke,  "")
            ("match",   match,   null)
            ("uri",     r.uri,   "");

        if (!created.empty())
        {
            bool ok = internal::parseRfc3339Timestamp(created, r.created);
            if (!ok)
            {
                throw error::Conversion("'created' property must be "
                                        "an RFC3339 timestamp");
            }
        }

        if (invoke.empty() || invoke == "single")
            r.invocationPolicy = InvocationPolicy::single;
        else if (invoke == "roundrobin")
            r.invocationPolicy = InvocationPolicy::roundRobin;
        else if (invoke == "random")
            r.invocationPolicy = InvocationPolicy::random;
        else if (invoke == "first")
            r.invocationPolicy = InvocationPolicy::first;
        else if (invoke == "last")
            r.invocationPolicy = InvocationPolicy::last;
        else
            r.invocationPolicy = InvocationPolicy::unknown;

        r.matchPolicy = internal::parseMatchPolicy(match);
    }

//------------------------------------------------------------------------------
CPPWAMP_INLINE RegistrationDetails::RegistrationDetails() {}

CPPWAMP_INLINE RegistrationDetails::RegistrationDetails(SessionIdList callees,
                                                        RegistrationInfo info)
    : callees(std::move(callees)),
      info(std::move(info))
{}

CPPWAMP_INLINE Object toObject(const RegistrationDetails& r)
{
    return Object
    {
        {"created", internal::toRfc3339Timestamp<6>(r.info.created)},
        {"id",      r.info.id},
        {"invoke",  "single"}, // TODO: Shared registrations
        {"match",   internal::toString(r.info.matchPolicy)},
        {"uri",     r.info.uri},
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE RegistrationLists::RegistrationLists() {}

CPPWAMP_INLINE Object toObject(const RegistrationLists& lists)
{
    return Object
    {
        {"exact",    lists.exact},
        {"prefix",   lists.prefix},
        {"wildcard", lists.exact},
    };
}

CPPWAMP_INLINE void convert(FromVariantConverter& conv, RegistrationLists& r)
{
    using List = RegistrationLists::List;

    conv("exact",    r.exact,  List{})
        ("prefix",   r.prefix, List{})
        ("wildcard", r.exact,  List{});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SubscriptionInfo::SubscriptionInfo() {}

CPPWAMP_INLINE SubscriptionInfo::SubscriptionInfo(
    Uri uri, TimePoint created, RegistrationId id, MatchPolicy p)
    : uri(std::move(uri)), created(created), id(id), matchPolicy(p)
{}

CPPWAMP_INLINE void convert(FromVariantConverter& conv, SubscriptionInfo& s)
{
    String created;
    Variant match;

    conv("created", created, "")
        ("id",      s.id,    0)
        ("match",   match,   null)
        ("uri",     s.uri,   "");

    if (!created.empty())
    {
        bool ok = internal::parseRfc3339Timestamp(created, s.created);
        if (!ok)
        {
            throw error::Conversion("'created' property must be "
                                    "an RFC3339 timestamp");
        }
    }

    s.matchPolicy = internal::parseMatchPolicy(match);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SubscriptionDetails::SubscriptionDetails() {}

CPPWAMP_INLINE SubscriptionDetails::SubscriptionDetails(SessionIdList s,
                                                        SubscriptionInfo i)
    : subscribers(std::move(s)),
      info(std::move(i))
{}

CPPWAMP_INLINE Object toObject(const SubscriptionDetails& s)
{
    return Object
    {
        {"created", internal::toRfc3339Timestamp<6>(s.info.created)},
        {"id",      s.info.id},
        {"match",   internal::toString(s.info.matchPolicy)},
        {"uri",     s.info.uri},
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SubscriptionLists::SubscriptionLists() {}

CPPWAMP_INLINE Object toObject(const SubscriptionLists& lists)
{
    return Object
    {
        {"exact",    lists.exact},
        {"prefix",   lists.prefix},
        {"wildcard", lists.exact},
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE RealmObserver::~RealmObserver() {}

CPPWAMP_INLINE void RealmObserver::onRealmClosed(const Uri&) {}

CPPWAMP_INLINE void RealmObserver::onJoin(const SessionDetails&) {}

CPPWAMP_INLINE void RealmObserver::onLeave(const SessionDetails&) {}

CPPWAMP_INLINE void RealmObserver::onRegister(const SessionDetails&,
                                              const RegistrationDetails&) {}

CPPWAMP_INLINE void RealmObserver::onUnregister(const SessionDetails&,
                                                const RegistrationDetails&) {}

CPPWAMP_INLINE void RealmObserver::onSubscribe(const SessionDetails&,
                                               const SubscriptionDetails&) {}

CPPWAMP_INLINE void RealmObserver::onUnsubscribe(const SessionDetails&,
                                                 const SubscriptionDetails&) {}

} // namespace wamp
