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
#include "config.hpp"
#include "cppwamp/sessioninfo.hpp"
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
    using Executor            = AnyIoExecutor;
    using FallbackExecutor    = AnyCompletionExecutor;
    using SessionIdList       = std::vector<SessionId>;
    using SubscriptionIdList  = std::vector<SubscriptionId>;
    using SessionHandler      = std::function<void (SessionDetails)>;
    using SessionFilter       = std::function<bool (SessionDetails)>;
    using RegistrationHandler = std::function<void (RegistrationDetails)>;
    using SubscriptionHandler = std::function<void (SubscriptionDetails)>;

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

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    countSessions(C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    countSessions(SessionFilter f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<SessionIdList, C>
    listSessions(C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<SessionIdList, C>
    listSessions(SessionFilter f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    forEachSession(SessionHandler f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SessionDetails>, C>
    lookupSession(SessionId sid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    killSessionById(SessionId sid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<bool>, C>
    killSessionById(SessionId sid, Reason r, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<SessionIdList, C>
    killSessions(SessionFilter f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<SessionIdList, C>
    killSessions(SessionFilter f, Reason r, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<RegistrationLists, C>
    listRegistrations(C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    forEachRegistration(MatchPolicy p, RegistrationHandler f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<RegistrationDetails>, C>
    lookupRegistration(Uri uri, MatchPolicy p, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<RegistrationDetails>, C>
    matchRegistration(Uri uri, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<RegistrationDetails>, C>
    getRegistration(RegistrationId rid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<SubscriptionLists, C>
    listSubscriptions(C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    forEachSubscription(MatchPolicy p, SubscriptionHandler f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SubscriptionDetails>, C>
    lookupSubscription(Uri uri, MatchPolicy p, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SubscriptionIdList>, C>
    matchSubscriptions(Uri uri, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SubscriptionDetails>, C>
    getSubscription(SubscriptionId rid, C&& completion);

private:
    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void (T)>;

    // These initiator function objects are needed due to C++11 lacking
    // generic lambdas.
    struct CountSessionsOp;
    struct ListSessionsOp;
    struct ForEachSessionOp;
    struct LookupSessionOp;
    struct KillSessionByIdOp;
    struct KillSessionsOp;
    struct ListRegistrationsOp;
    struct ForEachRegistrationOp;
    struct LookupRegistrationOp;
    struct MatchRegistrationOp;
    struct GetRegistrationOp;
    struct ListSubscriptionsOp;
    struct ForEachSubscriptionOp;
    struct LookupSubscriptionOp;
    struct MatchSubscriptionsOp;
    struct GetSubscriptionOp;

    template <typename O, typename C, typename... As>
    Deduced<typename O::ResultValue, C> initiate(C&& token, As&&... args);

    explicit Realm(std::shared_ptr<internal::RouterRealm> impl,
                   FallbackExecutor fe);

    template <typename F>
    typename internal::BindFallbackExecutorResult<F>::Type
    bindFallbackExecutor(F&& handler) const;

    void doCountSessions(SessionFilter f, CompletionHandler<std::size_t> h);
    void doListSessions(SessionFilter f, CompletionHandler<SessionIdList> h);
    void doForEachSession(SessionHandler f, CompletionHandler<std::size_t> h);
    void doLookupSession(SessionId sid,
                         CompletionHandler<ErrorOr<SessionDetails>> h);
    void doKillSessionById(SessionId sid, Reason r,
                           CompletionHandler<ErrorOr<bool>> h);
    void doKillSessions(SessionFilter f, Reason r,
                        CompletionHandler<SessionIdList> h);
    void doListRegistrations(CompletionHandler<RegistrationLists> h);
    void doForEachRegistration(MatchPolicy p, RegistrationHandler f,
                               CompletionHandler<std::size_t> h);
    void doLookupRegistration(Uri uri, MatchPolicy p,
                              CompletionHandler<ErrorOr<RegistrationDetails>> h);
    void doMatchRegistration(Uri uri,
                             CompletionHandler<ErrorOr<RegistrationDetails>> h);
    void doGetRegistration(RegistrationId rid,
                           CompletionHandler<ErrorOr<RegistrationDetails>> h);
    void doListSubscriptions(CompletionHandler<SubscriptionLists> h);
    void doForEachSubscription(MatchPolicy p, SubscriptionHandler f,
                               CompletionHandler<std::size_t> h);
    void doLookupSubscription(Uri uri, MatchPolicy p,
                              CompletionHandler<ErrorOr<SubscriptionDetails>> h);
    void doMatchSubscriptions(Uri uri, CompletionHandler<SubscriptionIdList> h);
    void doGetSubscription(SubscriptionId sid,
                           CompletionHandler<ErrorOr<SubscriptionDetails>> h);

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
struct Realm::CountSessionsOp
{
    using ResultValue = std::size_t;
    Realm* self;
    SessionFilter filter;

    template <typename F> void operator()(F&& f)
    {
        self->doCountSessions(std::move(filter),
                              self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (std::size_t)`,
              or a compatible Boost.Asio completion token.
    @return The number of active sessions meeting the filter criteria. */
//------------------------------------------------------------------------------
template <typename C>
Realm::Deduced<std::size_t, C> Realm::countSessions(
    C&& completion   /**< Completion handler or token. */
    )
{
    return countSessions(nullptr, std::forward<C>(completion));
}

//------------------------------------------------------------------------------
/** @copydetails Realm::countSessions(C&&) */
//------------------------------------------------------------------------------
template <typename C>
Realm::Deduced<std::size_t, C> Realm::countSessions(
    SessionFilter f, /**< Predicate function used to filter eligible sessions
                          (no filtering if nullptr) */
    C&& completion   /**< Completion handler or token. */
    )
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<CountSessionsOp>(std::forward<C>(completion), std::move(f));
}

//------------------------------------------------------------------------------
struct Realm::ListSessionsOp
{
    using ResultValue = Realm::SessionIdList;
    Realm* self;
    SessionFilter filter;

    template <typename F> void operator()(F&& f)
    {
        self->doListSessions(std::move(filter),
                             self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<Realm::SessionIdList, C>
Realm::listSessions(C&& completion)
{
    return listSessions(nullptr, std::forward<C>(completion));
}

template <typename C>
Realm::Deduced<Realm::SessionIdList, C>
Realm::listSessions(SessionFilter f, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<ListSessionsOp>(std::forward<C>(completion), std::move(f));
}

//------------------------------------------------------------------------------
struct Realm::ForEachSessionOp
{
    using ResultValue = std::size_t;
    Realm* self;
    SessionHandler onSession;

    template <typename F> void operator()(F&& f)
    {
        self->doForEachSession(std::move(onSession),
                               self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<std::size_t, C>
Realm::forEachSession(SessionHandler f, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<ForEachSessionOp>(std::forward<C>(completion),
                                      std::move(f));
}

//------------------------------------------------------------------------------
struct Realm::LookupSessionOp
{
    using ResultValue = ErrorOr<SessionDetails>;
    Realm* self;
    SessionId sid;

    template <typename F> void operator()(F&& f)
    {
        self->doLookupSession(sid,
                              self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (ErrorOr<SessionDetails>)`,
              or a compatible Boost.Asio completion token.
    @return The session's details if found, or an error otherwise. */
//------------------------------------------------------------------------------
template <typename C>
Realm::Deduced<ErrorOr<SessionDetails>, C>
Realm::lookupSession(SessionId sid, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<LookupSessionOp>(std::forward<C>(completion), sid);
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
    SessionFilter filter;
    Reason r;

    template <typename F> void operator()(F&& f)
    {
        self->doKillSessions(std::move(filter), std::move(r),
                             self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<Realm::SessionIdList, C>
Realm::killSessions(SessionFilter f, C&& completion)
{
    return killSessions(std::move(f), Reason{WampErrc::sessionKilled},
                        std::forward<C>(completion));
}

template <typename C>
Realm::Deduced<Realm::SessionIdList, C>
Realm::killSessions(SessionFilter f, Reason r, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<KillSessionsOp>(std::forward<C>(completion), std::move(f),
                                    std::move(r));
}

//------------------------------------------------------------------------------
struct Realm::ListRegistrationsOp
{
    using ResultValue = RegistrationLists;
    Realm* self;

    template <typename F> void operator()(F&& f)
    {
        self->doListRegistrations(
            self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<RegistrationLists, C>
Realm::listRegistrations(C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<ListRegistrationsOp>(std::forward<C>(completion));
}

//------------------------------------------------------------------------------
struct Realm::ForEachRegistrationOp
{
    using ResultValue = std::size_t;
    Realm* self;
    MatchPolicy p;
    RegistrationHandler onRegistration;

    template <typename F> void operator()(F&& f)
    {
        self->doForEachRegistration(
            p, std::move(onRegistration),
            self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<std::size_t, C>
Realm::forEachRegistration(MatchPolicy p, RegistrationHandler f, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<ForEachRegistrationOp>(std::forward<C>(completion), p,
                                           std::move(f));
}

//------------------------------------------------------------------------------
struct Realm::LookupRegistrationOp
{
    using ResultValue = ErrorOr<RegistrationDetails>;
    Realm* self;
    Uri uri;
    MatchPolicy p;

    template <typename F> void operator()(F&& f)
    {
        self->doLookupRegistration(
            std::move(uri), p, self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<RegistrationDetails>, C>
Realm::lookupRegistration(Uri uri, MatchPolicy p, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<LookupRegistrationOp>(std::forward<C>(completion),
                                          std::move(uri), p);
}

//------------------------------------------------------------------------------
struct Realm::MatchRegistrationOp
{
    using ResultValue = ErrorOr<RegistrationDetails>;
    Realm* self;
    Uri uri;

    template <typename F> void operator()(F&& f)
    {
        self->doMatchRegistration(
            std::move(uri), self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<RegistrationDetails>, C>
Realm::matchRegistration(Uri uri, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<MatchRegistrationOp>(std::forward<C>(completion),
                                         std::move(uri));
}

//------------------------------------------------------------------------------
struct Realm::GetRegistrationOp
{
    using ResultValue = ErrorOr<RegistrationDetails>;
    Realm* self;
    RegistrationId rid;

    template <typename F> void operator()(F&& f)
    {
        self->doGetRegistration(rid,
                                self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<RegistrationDetails>, C>
Realm::getRegistration(RegistrationId rid, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<GetRegistrationOp>(std::forward<C>(completion), rid);
}

//------------------------------------------------------------------------------
struct Realm::ListSubscriptionsOp
{
    using ResultValue = SubscriptionLists;
    Realm* self;

    template <typename F> void operator()(F&& f)
    {
        self->doListSubscriptions(
            self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<SubscriptionLists, C>
Realm::listSubscriptions(C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<ListSubscriptionsOp>(std::forward<C>(completion));
}

//------------------------------------------------------------------------------
struct Realm::ForEachSubscriptionOp
{
    using ResultValue = std::size_t;
    Realm* self;
    MatchPolicy p;
    SubscriptionHandler onSubscription;

    template <typename F> void operator()(F&& f)
    {
        self->doForEachSubscription(
            p, std::move(onSubscription),
            self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<std::size_t, C>
Realm::forEachSubscription(MatchPolicy p, SubscriptionHandler f, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<ForEachSubscriptionOp>(std::forward<C>(completion), p,
                                           std::move(f));
}

//------------------------------------------------------------------------------
struct Realm::LookupSubscriptionOp
{
    using ResultValue = ErrorOr<SubscriptionDetails>;
    Realm* self;
    Uri uri;
    MatchPolicy p;

    template <typename F> void operator()(F&& f)
    {
        self->doLookupSubscription(
            std::move(uri), p, self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<SubscriptionDetails>, C>
Realm::lookupSubscription(Uri uri, MatchPolicy p, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<LookupSubscriptionOp>(std::forward<C>(completion),
                                          std::move(uri), p);
}

//------------------------------------------------------------------------------
struct Realm::MatchSubscriptionsOp
{
    using ResultValue = Realm::SubscriptionIdList;
    Realm* self;
    Uri uri;

    template <typename F> void operator()(F&& f)
    {
        self->doMatchSubscriptions(
            std::move(uri), self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<Realm::SubscriptionIdList>, C>
Realm::matchSubscriptions(Uri uri, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<MatchSubscriptionsOp>(std::forward<C>(completion),
                                          std::move(uri));
}

//------------------------------------------------------------------------------
struct Realm::GetSubscriptionOp
{
    using ResultValue = ErrorOr<SubscriptionDetails>;
    Realm* self;
    SubscriptionId sid;

    template <typename F> void operator()(F&& f)
    {
        self->doGetSubscription(sid,
                                self->bindFallbackExecutor(std::forward<F>(f)));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<SubscriptionDetails>, C>
Realm::getSubscription(SubscriptionId sid, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(isAttached(), "Realm instance is unattached");
    return initiate<GetSubscriptionOp>(std::forward<C>(completion), sid);
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
