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
#include <set>
#include <type_traits>
#include <boost/asio/async_result.hpp>
#include "api.hpp"
#include "asiodefs.hpp"
#include "anyhandler.hpp"
#include "clientinfo.hpp"
#include "cppwamp/internal/routerrealm.hpp"
#include "erroror.hpp"
#include "realmobserver.hpp"

namespace wamp
{

// Forward declarations
class Router;

namespace internal
{
class RouterRealm;
class RouterSession;
}

//------------------------------------------------------------------------------
/** Provides management operations on a router realm. */
//------------------------------------------------------------------------------
class CPPWAMP_API Realm
{
public:
    using Executor              = AnyIoExecutor;
    using FallbackExecutor      = AnyCompletionExecutor;
    using SessionIdSet          = std::set<SessionId>;
    using SessionPredicate      = std::function<bool (SessionInfo)>;
    using RegistrationPredicate = std::function<bool (const RegistrationInfo&)>;
    using SubscriptionPredicate = std::function<bool (const SubscriptionInfo&)>;

    static Reason defaultKillReason();

    Realm();

    explicit operator bool() const;

    const Executor& executor() const;

    const FallbackExecutor& fallbackExecutor() const;

    const IoStrand& strand() const;

    const Uri& uri() const;

    bool isAttached() const;

    bool isOpen() const;

    bool close(Reason r = Reason{WampErrc::systemShutdown});

    void observe(RealmObserver::Ptr o);

    std::size_t sessionCount() const;

    std::size_t forEachSession(const SessionPredicate& handler) const;

    ErrorOr<SessionInfo> getSession(SessionId sid) const;

    ErrorOr<bool> killSessionById(SessionId sid,
                                  Reason r = defaultKillReason());

    SessionIdSet killSessionIf(const SessionPredicate& f,
                               Reason r = defaultKillReason());

    SessionIdSet killSessions(SessionIdSet set, Reason r = defaultKillReason());

    ErrorOr<RegistrationInfo> getRegistration(
        RegistrationId rid, bool listCallees = false) const;

    ErrorOr<RegistrationInfo> lookupRegistration(
        const Uri& uri, MatchPolicy p = MatchPolicy::exact,
        bool listCallees = false) const;

    ErrorOr<RegistrationInfo> bestRegistrationMatch(
        const Uri& uri, bool listCallees = false) const;

    std::size_t forEachRegistration(MatchPolicy p,
                                    const RegistrationPredicate& f) const;

    ErrorOr<SubscriptionInfo> getSubscription(
        SubscriptionId sid, bool listSubscribers = false) const;

    ErrorOr<SubscriptionInfo> lookupSubscription(
        const Uri& uri, MatchPolicy p = MatchPolicy::exact,
        bool listSubscribers = false) const;

    std::size_t forEachSubscription(MatchPolicy p,
                                    const SubscriptionPredicate& f) const;

    std::size_t forEachMatchingSubscription(
        const Uri& uri, const SubscriptionPredicate& f) const;

private:
    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void (T)>;

    explicit Realm(std::shared_ptr<internal::RouterRealm> impl,
                   FallbackExecutor fe);

    FallbackExecutor fallbackExecutor_;
    std::shared_ptr<internal::RouterRealm> impl_;

    friend class Router;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/realm.inl.hpp"
#endif

#endif // CPPWAMP_REALM_HPP
