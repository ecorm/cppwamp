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
#include "asiodefs.hpp"
#include "anyhandler.hpp"
#include "config.hpp"
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
private:
    struct GenericOp { template <typename F> void operator()(F&&) {} };

public:
    using Executor            = AnyIoExecutor;
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
    using Deduced = decltype(
        boost::asio::async_initiate<C, void(T)>(std::declval<GenericOp&>(),
                                                std::declval<C&>()));

    Realm();

    explicit operator bool() const;

    const Executor& executor() const;

    const IoStrand& strand() const;

    const Uri& uri() const;

    bool isOpen() const;

    void observe(RealmObserver::Ptr o);

    void unobserve();

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    countSessions(C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::vector<SessionId>, C>
    listSessions(C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    forEachSession(SessionHandler f, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<SessionDetails>, C>
    lookupSession(SessionId sid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<bool, C>
    killSession(SessionId sid, C&& completion);

    template <typename C>
    CPPWAMP_NODISCARD Deduced<std::size_t, C>
    killSessions(SessionFilter f, C&& completion);

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
    CPPWAMP_NODISCARD Deduced<ErrorOr<SubscriptionDetails>, C>
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
    struct KillSessionOp;
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

    explicit Realm(std::shared_ptr<internal::RouterRealm> impl);

    void doCountSessions(CompletionHandler<std::size_t> h);
    void doListSessions(CompletionHandler<std::vector<SessionId>> h);
    void doForEachSession(SessionHandler f, CompletionHandler<std::size_t> h);
    void doLookupSession(SessionId sid,
                         CompletionHandler<ErrorOr<SessionDetails>> h);
    void doKillSession(SessionId sid, CompletionHandler<bool> h);
    void doKillSessions(SessionFilter f, CompletionHandler<std::size_t> h);
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
    void doMatchSubscriptions(Uri uri,
                              CompletionHandler<std::vector<SubscriptionId>> h);
    void doGetSubscription(SubscriptionId sid,
                           CompletionHandler<ErrorOr<SubscriptionDetails>> h);

    std::shared_ptr<internal::RouterRealm> impl_;

    friend class Router;
};


//******************************************************************************
// Realm template member function definitions
//******************************************************************************

//------------------------------------------------------------------------------
struct Realm::CountSessionsOp
{
    using ResultValue = std::size_t;
    Realm* self;

    template <typename F> void operator()(F&& f)
    {
        self->doCountSessions(std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
/** @tparam C Callable handler with signature `void (std::size_t)`,
              or a compatible Boost.Asio completion token.
    @return The number of active sessions. */
//------------------------------------------------------------------------------
template <typename C>
Realm::Deduced<std::size_t, C> Realm::countSessions(
    C&& completion     ///< Completion handler or token.
    )
{
    return initiate<CountSessionsOp>(std::forward<C>(completion));
}

//------------------------------------------------------------------------------
struct Realm::ListSessionsOp
{
    using ResultValue = std::vector<SessionId>;
    Realm* self;

    template <typename F> void operator()(F&& f)
    {
        self->doListSessions(std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<std::vector<SessionId>, C>
Realm::listSessions(C&& completion)
{
    return initiate<ListSessionsOp>(std::forward<C>(completion));
}

//------------------------------------------------------------------------------
struct Realm::ForEachSessionOp
{
    using ResultValue = std::size_t;
    Realm* self;
    SessionHandler onSession;

    template <typename F> void operator()(F&& f)
    {
        self->doForEachSession(std::move(onSession), std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<std::size_t, C>
Realm::forEachSession(SessionHandler f, C&& completion)
{
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
        self->doLookupSession(sid, std::forward<F>(f));
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
    return initiate<LookupSessionOp>(std::forward<C>(completion), sid);
}

//------------------------------------------------------------------------------
struct Realm::KillSessionOp
{
    using ResultValue = std::size_t;
    Realm* self;
    SessionId sid;

    template <typename F> void operator()(F&& f)
    {
        self->doKillSession(sid, std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<bool, C>
Realm::killSession(SessionId sid, C&& completion)
{
    return initiate<KillSessionOp>(std::forward<C>(completion), sid);
}

//------------------------------------------------------------------------------
struct Realm::KillSessionsOp
{
    using ResultValue = std::size_t;
    Realm* self;
    SessionFilter filter;

    template <typename F> void operator()(F&& f)
    {
        self->doKillSessions(std::move(filter), std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<std::size_t, C>
Realm::killSessions(SessionFilter f, C&& completion)
{
    return initiate<KillSessionOp>(std::forward<C>(completion), std::move(f));
}

//------------------------------------------------------------------------------
struct Realm::ListRegistrationsOp
{
    using ResultValue = RegistrationLists;
    Realm* self;

    template <typename F> void operator()(F&& f)
    {
        self->doListRegistrations(std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<RegistrationLists, C>
Realm::listRegistrations(C&& completion)
{
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
        self->doForEachRegistration(p, std::move(onRegistration),
                                    std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<std::size_t, C>
Realm::forEachRegistration(MatchPolicy p, RegistrationHandler f, C&& completion)
{
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
        self->doLookupRegistration(std::move(uri), p, std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<RegistrationDetails>, C>
Realm::lookupRegistration(Uri uri, MatchPolicy p, C&& completion)
{
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
        self->doMatchRegistration(std::move(uri), std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<RegistrationDetails>, C>
Realm::matchRegistration(Uri uri, C&& completion)
{
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
        self->doGetRegistration(rid, std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<RegistrationDetails>, C>
Realm::getRegistration(RegistrationId rid, C&& completion)
{
    return initiate<GetRegistrationOp>(std::forward<C>(completion), rid);
}

//------------------------------------------------------------------------------
struct Realm::ListSubscriptionsOp
{
    using ResultValue = SubscriptionLists;
    Realm* self;

    template <typename F> void operator()(F&& f)
    {
        self->doListSubscriptions(std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<SubscriptionLists, C>
Realm::listSubscriptions(C&& completion)
{
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
        self->doForEachSubscription(p, std::move(onSubscription),
                                    std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<std::size_t, C>
Realm::forEachSubscription(MatchPolicy p, SubscriptionHandler f, C&& completion)
{
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
        self->doLookupSubscription(std::move(uri), p, std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<SubscriptionDetails>, C>
Realm::lookupSubscription(Uri uri, MatchPolicy p, C&& completion)
{
    return initiate<LookupSubscriptionOp>(std::forward<C>(completion),
                                          std::move(uri), p);
}

//------------------------------------------------------------------------------
struct Realm::MatchSubscriptionsOp
{
    using ResultValue = std::vector<SubscriptionId>;
    Realm* self;
    Uri uri;

    template <typename F> void operator()(F&& f)
    {
        self->doMatchSubscriptions(std::move(uri), std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<SubscriptionDetails>, C>
Realm::matchSubscriptions(Uri uri, C&& completion)
{
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
        self->doGetSubscription(sid, std::forward<F>(f));
    }
};

template <typename C>
Realm::Deduced<ErrorOr<SubscriptionDetails>, C>
Realm::getSubscription(SubscriptionId sid, C&& completion)
{
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
