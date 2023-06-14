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
#include <type_traits>
#include <vector>
#include <boost/asio/async_result.hpp>
#include "api.hpp"
#include "asiodefs.hpp"
#include "anyhandler.hpp"
#include "clientinfo.hpp"
#include "config.hpp"
#include "cppwamp/internal/routerrealm.hpp"
#include "erroror.hpp"
#include "exceptions.hpp"
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
    using SessionIdList         = std::vector<SessionId>;
    using SessionPredicate      = std::function<bool (const SessionInfo&)>;
    using RegistrationPredicate = std::function<bool (const RegistrationInfo&)>;
    using SubscriptionPredicate = std::function<bool (const SubscriptionInfo&)>;

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
    using Deduced = typename boost::asio::async_result<
        typename std::decay<C>::type, void(T)>::return_type;

    Realm();

    explicit operator bool() const;

    const Executor& executor() const;

    const FallbackExecutor& fallbackExecutor() const;

    const IoStrand& strand() const;

    const Uri& uri() const;

    bool isAttached() const;

    bool isOpen() const;

    void observe(RealmObserver::Ptr o);

    std::size_t sessionCount() const;

    std::size_t forEachSession(const SessionPredicate& handler) const;

    ErrorOr<SessionInfo::ConstPtr> lookupSession(SessionId sid) const;

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    killSessionById(SessionId sid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    killSessionById(SessionId sid, Reason r, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<SessionIdList, C>
    killSessions(SessionPredicate filter, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<SessionIdList, C>
    killSessions(SessionPredicate f, Reason r, C&& completion);

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

    // These initiator function objects are needed due to C++11 lacking
    // generic lambdas.
    struct KillSessionByIdOp;
    struct KillSessionsOp;

    template <typename O, typename C, typename... As>
    Deduced<typename O::ResultValue, C> initiate(C&& token, As&&... args);

    explicit Realm(std::shared_ptr<internal::RouterRealm> impl,
                   FallbackExecutor fe);

    template <typename F>
    typename internal::BindFallbackExecutorResult<F>::Type
    bindFallbackExecutor(F&& handler) const;

    void doKillSessionById(SessionId sid, Reason r,
                           CompletionHandler<ErrorOr<bool>> h);
    void doKillSessions(SessionPredicate f, Reason r,
                        CompletionHandler<SessionIdList> h);

    FallbackExecutor fallbackExecutor_;
    std::shared_ptr<internal::RouterRealm> impl_;

    friend class Router;
};


//******************************************************************************
// Realm template member function definitions
//******************************************************************************

//------------------------------------------------------------------------------
template <typename F>
typename internal::BindFallbackExecutorResult<F>::Type
Realm::bindFallbackExecutor(F&& handler) const
{
    return internal::bindFallbackExecutor(std::forward<F>(handler),
                                          fallbackExecutor_);
}

//------------------------------------------------------------------------------
struct Realm::KillSessionByIdOp
{
    using ResultValue = ErrorOr<bool>;
    Realm* self;
    SessionId sid;
    Reason r;

    template <typename F> void operator()(F&& f)
    {
        self->doKillSessionById(sid, std::move(r),
                            self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<bool>, C>
Realm::killSessionById(SessionId sid, C&& completion)
{
    return killSessionById(sid, Reason{WampErrc::sessionKilled},
                           std::forward<C>(completion));
}

template <typename C>
Realm::Deduced<ErrorOr<bool>, C>
Realm::killSessionById(SessionId sid, Reason r, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<KillSessionByIdOp>(std::forward<C>(completion), sid,
                                       std::move(r));
}

//------------------------------------------------------------------------------
struct Realm::KillSessionsOp
{
    using ResultValue = Realm::SessionIdList;
    Realm* self;
    SessionPredicate filter;
    Reason r;

    template <typename F> void operator()(F&& f)
    {
        self->doKillSessions(std::move(filter), std::move(r),
                             self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<Realm::SessionIdList, C>
Realm::killSessions(SessionPredicate filter, C&& completion)
{
    return killSessions(std::move(filter), Reason{WampErrc::sessionKilled},
                        std::forward<C>(completion));
}

template <typename C>
Realm::Deduced<Realm::SessionIdList, C>
Realm::killSessions(SessionPredicate f, Reason r, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<KillSessionsOp>(std::forward<C>(completion), std::move(f),
                                    std::move(r));
}

//------------------------------------------------------------------------------
template <typename O, typename C, typename... As>
Realm::template Deduced<typename O::ResultValue, C>
Realm::initiate(C&& token, As&&... args)
{
    return boost::asio::async_initiate<
        C, void(typename O::ResultValue)>(
        O{this, std::forward<As>(args)...}, token);
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/realm.ipp"
#endif

#endif // CPPWAMP_REALM_HPP
