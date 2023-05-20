/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_REALM_HPP
#define CPPWAMP_REALM_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities for managing a router realm. */
//------------------------------------------------------------------------------

#include <chrono>
#include <functional>
#include <memory>
#include <boost/asio/async_result.hpp>
#include "api.hpp"
#include "authinfo.hpp"
#include "config.hpp"
#include "erroror.hpp"
#include "features.hpp"
#include "wampdefs.hpp"

namespace wamp
{

// Forward declarations
namespace internal
{
class RouterRealm;
class RouterSession;
}

//------------------------------------------------------------------------------
/** Provides management operations on a router session. */
//------------------------------------------------------------------------------
class CPPWAMP_API SessionInfo
{
public:
    SessionId wampId() const;

    const AuthInfo& authInfo() const;

    ClientFeatures features() const;

private:
    std::shared_ptr<internal::RouterSession> impl_;
};


//------------------------------------------------------------------------------
struct RegistrationInfo
{
    std::vector<SessionId> callees;
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
};

//------------------------------------------------------------------------------
struct SubscriptionInfo
{
    std::vector<SessionId> subscribers;
    Uri uri;
    std::chrono::system_clock::time_point created;
    RegistrationId id;
    MatchPolicy matchPolicy;
};

//------------------------------------------------------------------------------
class CPPWAMP_API RealmObserver
{
public:
    using Ptr = std::shared_ptr<RealmObserver>;
    using WeakPtr = std::weak_ptr<RealmObserver>;

    virtual void onJoin(const SessionInfo&);

    virtual void onLeave(const SessionInfo&);

    virtual void onRegister(const SessionInfo&, const RegistrationInfo&,
                            std::size_t count);

    virtual void onUnregister(const SessionInfo&, const RegistrationInfo&,
                              std::size_t count);

    virtual void onSubscribe(const SessionInfo&, const SubscriptionInfo&,
                             std::size_t count);

    virtual void onUnsubscribe(const SessionInfo&, const SubscriptionInfo&,
                               std::size_t count);
};


//------------------------------------------------------------------------------
/** Provides management operations on a router realm. */
//------------------------------------------------------------------------------
class CPPWAMP_API Realm
{
private:
    struct GenericOp { template <typename F> void operator()(F&&) {} };

public:
    using SessionHandler = std::function<void (SessionInfo)>;
    using SessionFilter = std::function<bool (SessionInfo)>;
    using RegistrationHandler = std::function<void (RegistrationInfo)>;
    using SubscriptionHandler = std::function<void (SubscriptionInfo)>;

    /** Obtains the type returned by [boost::asio::async_initiate]
        (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/async_initiate.html)
        with given the completion token type `C` and signature `void(T)`.

        Token Type                   | Deduced Return Type
        ---------------------------- | -------------------
        Callback function            | `void`
        `wamp::YieldContext`         | `ErrorOr<Value>`
        `boost::asio::use_awaitable` | An awaitable yielding `ErrorOr<Value>`
        `boost::asio::use_future`    | `std::future<ErrorOr<Value>>` */
    template <typename T, typename C>
    using Deduced = decltype(
        boost::asio::async_initiate<C, void(T)>(std::declval<GenericOp&>(),
                                                std::declval<C&>()));

    const Uri& uri() const;

    void observe(RealmObserver::Ptr);

    void unobserve();

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    forEachSession(SessionHandler f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    killSession(SessionId sid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    killSessions(SessionFilter f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    forEachRegistration(MatchPolicy p, RegistrationHandler f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<RegistrationInfo>, C>
    findRegistration(Uri uri, MatchPolicy p, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<RegistrationInfo>, C>
    matchRegistration(Uri uri, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<RegistrationInfo>, C>
    getRegistration(RegistrationId rid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<std::size_t>, C>
    forEachSubscription(MatchPolicy p, SubscriptionHandler f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SubscriptionInfo>, C>
    findSubscription(Uri uri, MatchPolicy p, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SubscriptionInfo>, C>
    matchSubscription(Uri uri, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SubscriptionInfo>, C>
    getSubscription(SubscriptionId rid, C&& completion);

private:
    std::shared_ptr<internal::RouterRealm> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/realm.ipp"
#endif

#endif // CPPWAMP_REALM_HPP
