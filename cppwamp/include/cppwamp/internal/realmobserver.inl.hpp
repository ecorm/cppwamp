/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../realmobserver.hpp"
#include "../api.hpp"
#include "../utils/wildcarduri.hpp"
#include "matchpolicyoption.hpp"
#include "timeformatting.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Object toObject(const SessionInfo& info)
{
    return Object
    {
        {"authid",       info.auth().id()},
        {"authmethod",   info.auth().method()},
        {"authprovider", info.auth().provider()},
        {"authrole",     info.auth().role()},
        {"session",      info.sessionId()},
        {"transport",    info.connection().transport()}
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionJoinInfo::SessionJoinInfo() = default;

CPPWAMP_INLINE void convert(FromVariantConverter& conv, SessionJoinInfo& s)
{
    conv("authid",       s.authId,       "")
        ("authmethod",   s.authMethod,   "")
        ("authprovider", s.authProvider, "")
        ("authrole",     s.authRole,     "")
        ("session",      s.sessionId,    0)
        ("transport",    s.transport,    Object{});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionLeftInfo::SessionLeftInfo() = default;

CPPWAMP_INLINE SessionLeftInfo parseSessionLeftInfo(const Event& event)
{
    SessionLeftInfo s;
    s.sessionId = 0;
    event.convertTo(s.sessionId, s.authid, s.authrole);
    return s;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE RegistrationInfo::RegistrationInfo() = default;

CPPWAMP_INLINE RegistrationInfo::RegistrationInfo(
    Uri uri, MatchPolicy mp, InvocationPolicy ip, RegistrationId id,
    TimePoint created)
    : uri(std::move(uri)), created(created), id(id), matchPolicy(mp),
    invocationPolicy(ip)
{}

CPPWAMP_INLINE bool RegistrationInfo::matches(const Uri& procedure) const
{
    // TODO: Pattern-based registrations
    return procedure == uri;
}

CPPWAMP_INLINE void convertFrom(FromVariantConverter& conv,
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
            const bool ok = internal::parseRfc3339Timestamp(created, r.created);
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

CPPWAMP_INLINE void convertTo(ToVariantConverter& conv,
                              const RegistrationInfo& r)
{
    static constexpr unsigned precision = 6;

    conv("created", internal::toRfc3339Timestamp<precision>(r.created))
        ("id",      r.id)
        ("invoke",  "single") // TODO: Shared registrations
        ("match",   internal::toString(r.matchPolicy))
        ("uri",     r.uri);
}


//------------------------------------------------------------------------------
CPPWAMP_INLINE SubscriptionInfo::SubscriptionInfo() = default;

CPPWAMP_INLINE SubscriptionInfo::SubscriptionInfo(
    Uri uri, MatchPolicy p, SubscriptionId id, TimePoint created)
    : uri(std::move(uri)), created(created), id(id), matchPolicy(p)
{}

CPPWAMP_INLINE bool SubscriptionInfo::matches(const Uri& topic) const
{
    switch (matchPolicy)
    {
    case MatchPolicy::exact:    return topic == uri;
    case MatchPolicy::prefix:   return topic.rfind(uri, 0) == 0;
    case MatchPolicy::wildcard: return utils::matchesWildcardPattern(topic, uri);
    default: break;
    }

    assert(false && "Unexpected MatchPolicy enumerator");
    return false;
}

CPPWAMP_INLINE void convertFrom(FromVariantConverter& conv, SubscriptionInfo& s)
{
    String created;
    Variant match;

    conv("created", created, "")
        ("id",      s.id,    0)
        ("match",   match,   null)
        ("uri",     s.uri,   "");

    if (!created.empty())
    {
        const bool ok = internal::parseRfc3339Timestamp(created, s.created);
        if (!ok)
        {
            throw error::Conversion("'created' property must be "
                                    "an RFC3339 timestamp");
        }
    }

    s.matchPolicy = internal::parseMatchPolicy(match);
}

CPPWAMP_INLINE void convertTo(ToVariantConverter& conv,
                              const SubscriptionInfo& s)
{
    static constexpr unsigned precision = 6;

    conv("created", internal::toRfc3339Timestamp<precision>(s.created))
        ("id",      s.id)
        ("match",   internal::toString(s.matchPolicy))
        ("uri",     s.uri);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE RealmObserver::~RealmObserver()
{
    detach();
}

CPPWAMP_INLINE bool RealmObserver::isAttached() const
{
    return observerId_.load() != 0;
}

CPPWAMP_INLINE void RealmObserver::bindExecutor(AnyCompletionExecutor e)
{
    const std::lock_guard<std::mutex> guard(mutex_);
    executor_ = std::move(e);
}

CPPWAMP_INLINE void RealmObserver::detach()
{
    auto oid = observerId_.exchange(0);
    if (oid == 0)
        return;
    auto subject = subject_.lock();
    if (subject)
        subject->onDetach(oid);
}

CPPWAMP_INLINE void RealmObserver::onRealmClosed(const Uri&) {}

CPPWAMP_INLINE void RealmObserver::onJoin(const SessionInfo&) {}

CPPWAMP_INLINE void RealmObserver::onLeave(const SessionInfo&) {}

CPPWAMP_INLINE void RealmObserver::onRegister(const SessionInfo&,
                                              const RegistrationInfo&) {}

CPPWAMP_INLINE void RealmObserver::onUnregister(const SessionInfo&,
                                                const RegistrationInfo&) {}

CPPWAMP_INLINE void RealmObserver::onSubscribe(const SessionInfo&,
                                               const SubscriptionInfo&) {}

CPPWAMP_INLINE void RealmObserver::onUnsubscribe(const SessionInfo&,
                                                 const SubscriptionInfo&) {}

CPPWAMP_INLINE RealmObserver::RealmObserver() : observerId_(0) {}

CPPWAMP_INLINE RealmObserver::RealmObserver(AnyCompletionExecutor e)
    : executor_(std::move(e)),
      observerId_(0)
{}

CPPWAMP_INLINE void RealmObserver::onDetach(ObserverId oid) {}

CPPWAMP_INLINE void RealmObserver::attach(SubjectPtr d, ObserverId oid,
                                          const FallbackExecutor& e)
{
    const std::lock_guard<std::mutex> guard(mutex_);
    subject_ = std::move(d);
    observerId_.store(oid);
    if (!executor_)
        executor_ = e;
}

} // namespace wamp
