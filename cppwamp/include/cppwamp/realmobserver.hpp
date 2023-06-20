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
#include <set>
#include <vector>
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
    using SessionIdSet = std::set<SessionId>;

    RegistrationInfo();

    RegistrationInfo(Uri uri, MatchPolicy mp, InvocationPolicy ip,
                     RegistrationId id, TimePoint created);

    bool matches(const Uri& procedure) const;

    SessionIdSet callees;
    Uri uri;
    TimePoint created;
    RegistrationId id = 0;
    size_t calleeCount = 0;
    MatchPolicy matchPolicy = MatchPolicy::unknown;
    InvocationPolicy invocationPolicy = InvocationPolicy::unknown;
};

CPPWAMP_API void convertFrom(FromVariantConverter& conv, RegistrationInfo& r);
CPPWAMP_API void convertTo(ToVariantConverter& conv, const RegistrationInfo& r);
CPPWAMP_CONVERSION_SPLIT_FREE(RegistrationInfo)


//------------------------------------------------------------------------------
struct CPPWAMP_API SubscriptionInfo
{
    using TimePoint = std::chrono::system_clock::time_point;
    using SessionIdSet = std::set<SessionId>;

    SubscriptionInfo();

    SubscriptionInfo(Uri uri, MatchPolicy p, SubscriptionId id,
                     TimePoint created);

    bool matches(const Uri& topic) const;

    SessionIdSet subscribers;
    Uri uri;
    TimePoint created;
    SubscriptionId id = 0;
    size_t subscriberCount = 0;
    MatchPolicy matchPolicy = MatchPolicy::unknown;
};

CPPWAMP_API void convertFrom(FromVariantConverter& conv, SubscriptionInfo& s);
CPPWAMP_API void convertTo(ToVariantConverter& conv, const SubscriptionInfo& s);
CPPWAMP_CONVERSION_SPLIT_FREE(SubscriptionInfo)


//------------------------------------------------------------------------------
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

    virtual void onJoin(SessionInfo);

    virtual void onLeave(SessionInfo);

    virtual void onRegister(SessionInfo, RegistrationInfo);

    virtual void onUnregister(SessionInfo, RegistrationInfo);

    virtual void onSubscribe(SessionInfo, SubscriptionInfo);

    virtual void onUnsubscribe(SessionInfo, SubscriptionInfo);

    RealmObserver(const RealmObserver&) = delete;
    RealmObserver(RealmObserver&&) = delete;
    RealmObserver& operator=(const RealmObserver&) = delete;
    RealmObserver& operator=(RealmObserver&&) = delete;

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
#include "internal/realmobserver.inl.hpp"
#endif

#endif // CPPWAMP_REALMOBSERVER_HPP
