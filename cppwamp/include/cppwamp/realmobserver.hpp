/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_REALMOBSERVER_HPP
#define CPPWAMP_REALMOBSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities for obtaining realm information. */
//------------------------------------------------------------------------------

#include <chrono>
#include "api.hpp"
#include "authinfo.hpp"
#include "features.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct CPPWAMP_API SessionDetails
{
    SessionDetails();

    SessionDetails(ClientFeatures f, AuthInfo::Ptr a, SessionId i);

    ClientFeatures features;
    AuthInfo::Ptr authInfo;
    SessionId id = 0;
};

CPPWAMP_API Object toObject(const SessionDetails& details);

//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationDetails
{
    std::vector<SessionId> callees;
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
};

CPPWAMP_API Object toObject(const RegistrationDetails& r);

//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationLists
{
    std::vector<RegistrationId> exact;
    std::vector<RegistrationId> prefix;
    std::vector<RegistrationId> wildcard;
};

CPPWAMP_API Object toObject(const RegistrationLists& lists);

//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionDetails
{
    std::vector<SessionId> subscribers;
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
};

CPPWAMP_API Object toObject(const SubscriptionDetails& s);

//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionLists
{
    std::vector<SubscriptionId> exact;
    std::vector<SubscriptionId> prefix;
    std::vector<SubscriptionId> wildcard;
};

CPPWAMP_API Object toObject(const SubscriptionLists& lists);

//------------------------------------------------------------------------------
class CPPWAMP_API RealmObserver
{
public:
    // TODO: Bit mask for events of interest

    using Ptr = std::shared_ptr<RealmObserver>;
    using WeakPtr = std::weak_ptr<RealmObserver>;

    virtual ~RealmObserver();

    virtual void onRealmClosed(const Uri&);

    virtual void onJoin(const SessionDetails&);

    virtual void onLeave(const SessionDetails&);

    virtual void onRegister(const SessionDetails&,
                            const RegistrationDetails&);

    virtual void onUnregister(const SessionDetails&,
                              const RegistrationDetails&);

    virtual void onSubscribe(const SessionDetails&,
                             const SubscriptionDetails&);

    virtual void onUnsubscribe(const SessionDetails&,
                               const SubscriptionDetails&);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/realmobserver.ipp"
#endif

#endif // CPPWAMP_REALMOBSERVER_HPP
