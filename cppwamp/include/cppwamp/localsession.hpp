/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LOCALSESSION_HPP
#define CPPWAMP_LOCALSESSION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the LocalSession class. */
//------------------------------------------------------------------------------

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include "api.hpp"
#include "anyhandler.hpp"
#include "asiodefs.hpp"
#include "chits.hpp"
#include "peerdata.hpp"
#include "registration.hpp"
#include "router.hpp"
#include "subscription.hpp"
#include "tagtypes.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class LocalSessionImpl; }


//------------------------------------------------------------------------------
class CPPWAMP_API LocalSession
{
private:
    struct GenericOp { template <typename F> void operator()(F&&) {} };

public:
    /** Executor type used for I/O operations. */
    using Executor = AnyIoExecutor;

    /** Fallback executor type for user-provided handlers. */
    using FallbackExecutor = AnyCompletionExecutor;

    /** Type-erased wrapper around a WAMP event handler. */
    using EventSlot = AnyReusableHandler<void (Event)>;

    /** Type-erased wrapper around an RPC handler. */
    using CallSlot = AnyReusableHandler<Outcome (Invocation)>;

    /** Type-erased wrapper around an RPC interruption handler. */
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;

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

    LocalSession();

    /// @name Observers
    /// @{
    /** Return true if the local session is expired due to its bound realm
        being shut down. */
    bool expired() const;
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    Subscription subscribe(Topic topic, EventSlot eventSlot);

    /** Thread-safe subscribe. */
    std::future<Subscription> subscribe(ThreadSafe, Topic topic,
                                        EventSlot eventSlot);

    /** Unsubscribes a subscription to a topic. */
    void unsubscribe(Subscription sub);

    /** Thread-safe unsubscribe. */
    void unsubscribe(ThreadSafe, Subscription sub);

    /** Publishes an event. */
    PublicationId publish(Pub pub);

    /** Thread-safe publish. */
    std::future<PublicationId> publish(ThreadSafe, Pub pub);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    Registration enroll(Procedure procedure, CallSlot callSlot);

    /** Thread-safe enroll. */
    std::future<Registration> enroll(ThreadSafe, Procedure procedure,
                                     CallSlot callSlot);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    Registration enroll(Procedure procedure, CallSlot callSlot,
                        InterruptSlot interruptSlot);

    /** Thread-safe enroll interruptible. */
    std::future<Registration>
    enroll(ThreadSafe, Procedure procedure, CallSlot callSlot,
           InterruptSlot interruptSlot);

    /** Unregisters a remote procedure call. */
    void unregister(Registration reg);

    /** Thread-safe unregister. */
    void unregister(ThreadSafe, Registration reg);

    /** Calls a remote procedure. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(Rpc rpc, C&& completion);

    /** Thread-safe call. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(ThreadSafe, Rpc rpc, C&& completion);

    /** Calls a remote procedure, obtaining a token that can be used
        for cancellation. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(Rpc rpc, CallChit& chit, C&& completion);

    /** Thread-safe call with CallChit capture. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion);

    /** Calls a remote procedure with progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(Rpc rpc, C&& completion);

    /** Thread-safe call with progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(ThreadSafe, Rpc rpc, C&& completion);

    /** Calls a remote procedure with progressive results, obtaining a token
        that can be used for cancellation. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(Rpc rpc, CallChit& chit, C&& completion);

    /** Thread-safe call with CallChit capture and progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion);

    /** Cancels a remote procedure using the cancel mode that was specified
        in the @ref wamp::Rpc "Rpc". */
    bool cancel(CallChit);

    /** Thread-safe cancel. */
    std::future<bool> cancel(ThreadSafe, CallChit chit);

    /** Cancels a remote procedure using the given mode. */
    bool cancel(CallChit chit, CallCancelMode mode);

    /** Thread-safe cancel with a given mode. */
    std::future<bool> cancel(ThreadSafe, CallChit, CallCancelMode mode);
    /// @}

private:
    using ImplPtr = std::shared_ptr<internal::LocalSessionImpl>;

    template <typename T>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<T>)>;

    using OngoingCallHandler = AnyReusableHandler<void(ErrorOr<Result>)>;

    struct CallOp;
    struct OngoingCallOp;

    explicit LocalSession(ImplPtr impl);

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    initiate(C&& token, As&&... args);

    template <typename O, typename C, typename... As>
    Deduced<ErrorOr<typename O::ResultValue>, C>
    safelyInitiate(C&& token, As&&... args);

    void doOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f);
    void safeOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f);
    void doOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f);
    void safeOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f);

    ImplPtr impl_;

    friend class Router;
};


//------------------------------------------------------------------------------
struct LocalSession::CallOp
{
    using ResultValue = Result;
    LocalSession* self;
    Rpc r;
    CallChit* c;

    template <typename F> void operator()(F&& f)
    {
        self->doOneShotCall(std::move(r), c, std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeOneShotCall(std::move(r), c, std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::call(Rpc rpc, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use LocalSession::ongoingCall for progressive results");
    return initiate<CallOp>(std::forward<C>(completion), std::move(rpc),
                            nullptr);
}

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::call(ThreadSafe, Rpc rpc, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use LocalSession::ongoingCall for progressive results");
    return safelyInitiate<CallOp>(std::forward<C>(completion), std::move(rpc),
                                  nullptr);
}

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::call(Rpc rpc, CallChit& chit, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use LocalSession::ongoingCall for progressive results");
    return initiate<CallOp>(std::forward<C>(completion), std::move(rpc), &chit);
}

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::call(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion)
{
    CPPWAMP_LOGIC_CHECK(!rpc.progressiveResultsAreEnabled(),
                        "Use LocalSession::ongoingCall for progressive results");
    return safelyInitiate<CallOp>(std::forward<C>(completion), std::move(rpc),
                                  &chit);
}

//------------------------------------------------------------------------------
struct LocalSession::OngoingCallOp
{
    using ResultValue = Result;
    LocalSession* self;
    Rpc r;
    CallChit* c;

    template <typename F> void operator()(F&& f)
    {
        self->doOngoingCall(std::move(r), c, std::forward<F>(f));
    }

    template <typename F> void operator()(F&& f, ThreadSafe)
    {
        self->safeOngoingCall(std::move(r), c, std::forward<F>(f));
    }
};

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::ongoingCall(Rpc rpc, C&& completion)
{
    return initiate<OngoingCallOp>(std::forward<C>(completion), std::move(rpc),
                                   nullptr);
}

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::ongoingCall(ThreadSafe, Rpc rpc, C&& completion)
{
    return safelyInitiate<OngoingCallOp>(std::forward<C>(completion),
                                         std::move(rpc), nullptr);
}

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::ongoingCall(Rpc rpc, CallChit& chit, C&& completion)
{
    return initiate<OngoingCallOp>(std::forward<C>(completion), std::move(rpc),
                                   &chit);
}

//------------------------------------------------------------------------------
template <typename C>
LocalSession::template Deduced<ErrorOr<Result>, C>
LocalSession::ongoingCall(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion)
{
    return safelyInitiate<OngoingCallOp>(std::forward<C>(completion),
                                         std::move(rpc), &chit);
}

//------------------------------------------------------------------------------
template <typename O, typename C, typename... As>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<typename O::ResultValue>, C>
#else
LocalSession::template Deduced<ErrorOr<typename O::ResultValue>, C>
#endif
LocalSession::initiate(C&& token, As&&... args)
{
    return boost::asio::async_initiate<
        C, void(ErrorOr<typename O::ResultValue>)>(
        O{this, std::forward<As>(args)...}, token);
}

//------------------------------------------------------------------------------
template <typename O, typename C, typename... As>
#ifdef CPPWAMP_FOR_DOXYGEN
Deduced<ErrorOr<typename O::ResultValue>, C>
#else
LocalSession::template Deduced<ErrorOr<typename O::ResultValue>, C>
#endif
LocalSession::safelyInitiate(C&& token, As&&... args)
{
    return boost::asio::async_initiate<
        C, void(ErrorOr<typename O::ResultValue>)>(
        O{this, std::forward<As>(args)...}, token, threadSafe);
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/localsession.ipp"
#endif

#endif // CPPWAMP_LOCALSESSION_HPP
