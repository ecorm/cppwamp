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

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include "anyhandler.hpp"
#include "api.hpp"
#include "pubsubinfo.hpp"
#include "sessioninfo.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_API Object toObject(const SessionInfo& info);


//------------------------------------------------------------------------------
struct CPPWAMP_API SessionJoinInfo
{
    SessionJoinInfo();

    Object transport;
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

namespace internal {class MetaTopics;}

//------------------------------------------------------------------------------
class CPPWAMP_API RealmObserver
    : public std::enable_shared_from_this<RealmObserver>
{
public:
    // TODO: Bit mask for events of interest

    using Ptr = std::shared_ptr<RealmObserver>;
    using WeakPtr = std::weak_ptr<RealmObserver>;

    virtual ~RealmObserver();

    bool isAttached() const;

    void bindExecutor(AnyCompletionExecutor e);

    void detach();

    virtual void onRealmClosed(Uri);

    virtual void onJoin(SessionInfo::ConstPtr);

    virtual void onLeave(SessionInfo::ConstPtr);

    virtual void onRegister(SessionInfo::ConstPtr, RegistrationDetails);

    virtual void onUnregister(SessionInfo::ConstPtr, RegistrationDetails);

    virtual void onSubscribe(SessionInfo::ConstPtr, SubscriptionDetails);

    virtual void onUnsubscribe(SessionInfo::ConstPtr, SubscriptionDetails);

protected:
    RealmObserver();

    explicit RealmObserver(AnyCompletionExecutor e);

private:
    using ObserverId = uint64_t;
    using SubjectPtr = std::weak_ptr<RealmObserver>;
    using FallbackExecutor = AnyCompletionExecutor;

    virtual void onDetach(ObserverId oid);

    void attach(SubjectPtr d, ObserverId oid, const FallbackExecutor& e);

    template <typename E, typename F>
    void notify(E&& executionContext, F&& notifier)
    {
        if (!isAttached())
            return;

        AnyCompletionExecutor e;
        {
            std::lock_guard<std::mutex> guard{mutex_};
            e = executor_;
        }
        assert(e != nullptr);

        boost::asio::post(
            executionContext,
            boost::asio::bind_executor(e, std::forward<F>(notifier)));
    }

    std::mutex mutex_;
    AnyCompletionExecutor executor_;
    SubjectPtr subject_;
    std::atomic<ObserverId> observerId_;

    friend class internal::MetaTopics;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/realmobserver.ipp"
#endif

#endif // CPPWAMP_REALMOBSERVER_HPP
