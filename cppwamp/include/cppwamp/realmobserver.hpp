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
#include "pubsubinfo.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct CPPWAMP_API SessionDetails
{
    ClientFeatures features;
    AuthInfo::Ptr authInfo;
};

CPPWAMP_API Object toObject(const SessionDetails& details);


//------------------------------------------------------------------------------
struct CPPWAMP_API SessionJoinInfo
{
    Object transport; // TODO
    String authid;
    String authmethod;
    String authprovider;
    String authrole;
    SessionId sessionId;
};

CPPWAMP_API void convert(FromVariantConverter& conv, SessionJoinInfo& s);


//------------------------------------------------------------------------------
struct CPPWAMP_API SessionLeftInfo
{
    String authid;
    String authrole;
    SessionId sessionId;
};

CPPWAMP_API SessionLeftInfo parseSessionLeftInfo(const Event& event);

//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationInfo
{
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
    InvocationPolicy invocationPolicy;
};

CPPWAMP_API void convert(FromVariantConverter& conv, RegistrationInfo& r);


//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationDetails
{
    std::vector<SessionId> callees;
    RegistrationInfo info;
};

CPPWAMP_API Object toObject(const RegistrationDetails& r);


//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationLists
{
    using List = std::vector<RegistrationId>;

    List exact;
    List prefix;
    List wildcard;
};

CPPWAMP_API Object toObject(const RegistrationLists& lists);

CPPWAMP_API void convert(FromVariantConverter& conv, RegistrationLists& r);


//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionInfo
{
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
};

CPPWAMP_API void convert(FromVariantConverter& conv, SubscriptionInfo& s);

//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionDetails
{
    std::vector<SessionId> subscribers;
    SubscriptionInfo info;
};

CPPWAMP_API Object toObject(const SubscriptionDetails& s);


//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionLists
{
    using List = std::vector<SubscriptionId>;

    List exact;
    List prefix;
    List wildcard;
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
