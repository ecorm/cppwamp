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
#include "wampdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct CPPWAMP_API SessionDetails
{
    ClientFeatures features;
    AuthInfo::Ptr authInfo;
    SessionId id;
};

//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationDetails
{
    std::vector<SessionId> callees;
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
};

//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationLists
{
    std::vector<RegistrationId> exact;
    std::vector<RegistrationId> prefix;
    std::vector<RegistrationId> wildcard;
};

//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionDetails
{
    std::vector<SessionId> subscribers;
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
};

//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionLists
{
    std::vector<SubscriptionId> exact;
    std::vector<SubscriptionId> prefix;
    std::vector<SubscriptionId> wildcard;
};

//------------------------------------------------------------------------------
class CPPWAMP_API RealmObserver
{
public:
    using Ptr = std::shared_ptr<RealmObserver>;
    using WeakPtr = std::weak_ptr<RealmObserver>;

    virtual ~RealmObserver() {}

    virtual void onJoin(const SessionDetails&) {}

    virtual void onLeave(const SessionDetails&) {}

    virtual void onRegister(const SessionDetails&, const RegistrationDetails&,
                            std::size_t count) {}

    virtual void onUnregister(const SessionDetails&, const RegistrationDetails&,
                              std::size_t count) {}

    virtual void onSubscribe(const SessionDetails&, const SubscriptionDetails&,
                             std::size_t count) {}

    virtual void onUnsubscribe(const SessionDetails&,
                               const SubscriptionDetails&, std::size_t count) {}
};

} // namespace wamp

#endif // CPPWAMP_REALMOBSERVER_HPP
