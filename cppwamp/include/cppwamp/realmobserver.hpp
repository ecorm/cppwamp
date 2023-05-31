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
    SessionDetails();

    SessionDetails(ClientFeatures f, AuthInfo a);

    ClientFeatures features;
    AuthInfo authInfo;
};

CPPWAMP_API Object toObject(const SessionDetails& details);


//------------------------------------------------------------------------------
struct CPPWAMP_API SessionJoinInfo
{
    SessionJoinInfo();

    Object transport; // TODO
    String authId;
    String authMethod;
    String authProvider;
    String authRole;
    SessionId sessionId = 0;
};

CPPWAMP_API void convert(FromVariantConverter& conv, SessionJoinInfo& s);


//------------------------------------------------------------------------------
struct CPPWAMP_API SessionLeftInfo
{
    SessionLeftInfo();

    String authid;
    String authrole;
    SessionId sessionId = 0;
};

CPPWAMP_API SessionLeftInfo parseSessionLeftInfo(const Event& event);

//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationInfo
{
    using TimePoint = std::chrono::system_clock::time_point;

    RegistrationInfo();

    RegistrationInfo(Uri uri, TimePoint created, RegistrationId id,
                     MatchPolicy mp = MatchPolicy::exact,
                     InvocationPolicy ip = InvocationPolicy::single);

    Uri uri;
    TimePoint created;
    RegistrationId id = 0;
    MatchPolicy matchPolicy = MatchPolicy::unknown;
    InvocationPolicy invocationPolicy = InvocationPolicy::unknown;
};

CPPWAMP_API void convert(FromVariantConverter& conv, RegistrationInfo& r);


//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationDetails
{
    using SessionIdList = std::vector<SessionId>;

    RegistrationDetails();

    RegistrationDetails(SessionIdList callees, RegistrationInfo info);

    SessionIdList callees;
    RegistrationInfo info;
};

CPPWAMP_API Object toObject(const RegistrationDetails& r);


//------------------------------------------------------------------------------
struct CPPWAMP_API RegistrationLists
{
    using List = std::vector<RegistrationId>;

    RegistrationLists();

    List exact;
    List prefix;
    List wildcard;
};

CPPWAMP_API Object toObject(const RegistrationLists& lists);

CPPWAMP_API void convert(FromVariantConverter& conv, RegistrationLists& r);


//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionInfo
{
    using TimePoint = std::chrono::system_clock::time_point;

    SubscriptionInfo();

    SubscriptionInfo(Uri uri, TimePoint created, RegistrationId id,
                     MatchPolicy p = MatchPolicy::exact);

    Uri uri;
    TimePoint created;
    RegistrationId id = 0;
    MatchPolicy matchPolicy = MatchPolicy::unknown;
};

CPPWAMP_API void convert(FromVariantConverter& conv, SubscriptionInfo& s);

//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionDetails
{
    using SessionIdList = std::vector<SessionId>;

    SubscriptionDetails();

    SubscriptionDetails(SessionIdList s, SubscriptionInfo i);

    SessionIdList subscribers;
    SubscriptionInfo info;
};

CPPWAMP_API Object toObject(const SubscriptionDetails& s);


//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionLists
{
    using List = std::vector<SubscriptionId>;

    SubscriptionLists();

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

    virtual void onRealmClosed(Uri);

    virtual void onJoin(SessionDetails);

    virtual void onLeave(SessionDetails);

    virtual void onRegister(SessionDetails, RegistrationDetails);

    virtual void onUnregister(SessionDetails, RegistrationDetails);

    virtual void onSubscribe(SessionDetails, SubscriptionDetails);

    virtual void onUnsubscribe(SessionDetails, SubscriptionDetails);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/realmobserver.ipp"
#endif

#endif // CPPWAMP_REALMOBSERVER_HPP
