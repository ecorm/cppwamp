/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ANYHANDLER_HPP
#define CPPWAMP_ANYHANDLER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for type-erasing asynchronous handlers. */
//------------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <memory>
#include <functional>
#include <utility>
#include <tuple>
#include <boost/asio/any_completion_executor.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Type-erases an executor that is to be used with type-erased handlers. */
//------------------------------------------------------------------------------
using AnyCompletionExecutor = boost::asio::any_completion_executor;


//------------------------------------------------------------------------------
/** Type-erases a one-shot (and possibly move-only) asynchronous completion
    handler.
    The executor associated with the type-erased handler can be obtained via
    [boost::asio::get_associated_executor]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/get_associated_executor.html).
    @see AnyCompletionExecutor
    @see AnyReusableHandler */
//------------------------------------------------------------------------------
template <typename TSignature>
using AnyCompletionHandler = boost::asio::any_completion_handler<TSignature>;


//------------------------------------------------------------------------------
/** Type-erases a multi-shot, copyable callback handler.
    The executor associated with the type-erased handler can be obtained via
    [boost::asio::get_associated_executor]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/get_associated_executor.html).
    @see AnyCompletionExecutor
    @see AnyCompletionHandler */
//------------------------------------------------------------------------------
template <typename TSignature>
class AnyReusableHandler
{
private:
    using Function = std::function<TSignature>;

    template <typename F>
    static constexpr bool fnConstructible() noexcept
    {
        using Decayed = typename std::decay<F>::type;
        return !isSameType<Decayed, AnyReusableHandler>() &&
               !isSameType<Decayed, std::nullptr_t>() &&
               std::is_constructible<Function, F>::value;
    }

    template <typename F>
    static constexpr bool otherConstructible() noexcept
    {
        return std::is_constructible<Function, std::function<F>>::value;
    }

public:
    /** Polymorphic wrapper around the executor associated with this handler. */
    using Executor = AnyCompletionExecutor;

    /** Cancellation slot type used by this handler. */
    using CancellationSlot = boost::asio::cancellation_slot;

    /** Asio-conformant type alias. */
    using executor_type = Executor;

    /** Asio-conformant type alias. */
    using cancellation_slot_type = CancellationSlot;

    /** Default constructor. */
    AnyReusableHandler() = default;

    /** Copy constructor. */
    AnyReusableHandler(const AnyReusableHandler&) = default;

    /** Move constructor. */
    AnyReusableHandler(AnyReusableHandler&&) noexcept = default;

    /** Constructor copying another AnyReusableHandler with a different
        signature.
        Participates in overload resolution when
        `std::is_constructible_v<std::function<TSignature>, std::function<S>>`
        is true. */
    template <typename S, CPPWAMP_NEEDS(otherConstructible<S>()) = 0>
    AnyReusableHandler(const AnyReusableHandler<S>& rhs)
        : executor_(rhs.executor_),
          handler_(rhs.handler_)
    {}

    /** Constructor moving another AnyReusableHandler with a different
        signature.
        Participates in overload resolution when
        `std::is_constructible_v<std::function<TSignature>, std::function<S>>`
        is true. */
    template <typename S, CPPWAMP_NEEDS(otherConstructible<S>()) = 0>
    AnyReusableHandler(AnyReusableHandler<S>&& rhs) noexcept
        : executor_(std::move(rhs.executor_)),
          handler_(std::move(rhs.handler_))
    {}

    /** Constructor taking a callable entity.
        Participates in overload resolution when
        - `std::is_same_v<std::decay_t<F>, AnyReusableHandler<TSignature>>` is
           false, and,
        - `std::is_same_v<std::decay_t<F>, std::nullptr_t` is false, and,
        - `std::is_constructible_v<std::function<TSignature>, F>` is true. */
    template <typename F, CPPWAMP_NEEDS(fnConstructible<F>()) = 0>
    AnyReusableHandler(F&& handler)
        : executor_(boost::asio::get_associated_executor(
                        handler, AnyCompletionExecutor{})),
          cancelSlot_(boost::asio::get_associated_cancellation_slot(handler)),
          handler_(std::forward<F>(handler))
    {}

    /** Constructs an empty AnyReusableHandler. */
    AnyReusableHandler(std::nullptr_t) noexcept {}

    /** Renders an AnyReusableHandler empty. */
    AnyReusableHandler& operator=(std::nullptr_t) noexcept
    {
        executor_ = nullptr;
        handler_ = nullptr;
        return *this;
    }

    /** Copy assignment. */
    AnyReusableHandler& operator=(const AnyReusableHandler&) = default;

    /** Move assignment. */
    AnyReusableHandler& operator=(AnyReusableHandler&&) noexcept = default;

    /** Swaps contents with another AnyReusableHandler. */
    void swap(AnyReusableHandler& rhs) noexcept
    {
        // boost::asio::executor does not have member swap
        using std::swap;
        swap(executor_, rhs.executor_);

        handler_.swap(rhs.handler_);
    }

    /** Returns false iff the AnyReusableHandler is empty. */
    explicit operator bool() const noexcept {return bool(handler_);}

    /** Obtains the executor associated with this handler. */
    const Executor& get_executor() const {return executor_;}

    /** Obtains the cancellation slot associated with this handler. */
    const CancellationSlot& get_cancellation_slot() const {return cancelSlot_;}

    /** Invokes the handler with the given arguments. */
    template <typename... Ts>
    auto operator()(Ts&&... args) const
        -> decltype(std::declval<Function>()(std::forward<Ts>(args)...))
    {
        return handler_(std::forward<Ts>(args)...);
    }

private:
    Executor executor_;
    CancellationSlot cancelSlot_;
    Function handler_;
};

/** Non-member swap. @relates AnyReusableHandler */
template <typename S>
void swap(AnyReusableHandler<S>& a, AnyReusableHandler<S>& b) noexcept
{
    return a.swap(b);
}

/** Returns true is the given handler is empty. @relates AnyReusableHandler */
template <typename S>
bool operator==(const AnyReusableHandler<S>& f, std::nullptr_t) noexcept
{
    return !f;
}

/** Returns true is the given handler is empty. @relates AnyReusableHandler */
template <typename S>
bool operator==(std::nullptr_t, const AnyReusableHandler<S>& f) noexcept
{
    return !f;
}

/** Returns false is the given handler is empty. @relates AnyReusableHandler */
template <typename S>
bool operator!=(const AnyReusableHandler<S>& f, std::nullptr_t) noexcept
{
    return bool(f);
}

/** Returns false is the given handler is empty. @relates AnyReusableHandler */
template <typename S>
bool operator!=(std::nullptr_t, const AnyReusableHandler<S>& f) noexcept
{
    return bool(f);
}

} // namespace wamp


namespace boost
{
namespace asio
{

// Enable boost::asio::get_associated_executor for AnyReusableHandler.
template <typename S, typename E>
struct associated_executor<wamp::AnyReusableHandler<S>, E>
{
    using type = typename wamp::AnyReusableHandler<S>::Executor;

    static type get(const wamp::AnyReusableHandler<S>& f, const E& e = E{})
    {
        return f.get_executor() ? f.get_executor() : e;
    }
};

// Enable boost::asio::get_associated_cancellation_slot for AnyReusableHandler.
template <typename S, typename C>
struct associated_cancellation_slot<wamp::AnyReusableHandler<S>, C>
{
    using type = typename wamp::AnyReusableHandler<S>::CancellationSlot;

    static type get(const wamp::AnyReusableHandler<S>& f, const C& = C{})
    {
        return f.get_cancellation_slot();
    }
};

// Enable boost::asio::get_associated_allocator for AnyReusableHandler.
template <typename S, typename A>
struct associated_allocator<wamp::AnyReusableHandler<S>, A>
{
    using type = A;

    static type get(const wamp::AnyReusableHandler<S>&, const A& a = A{})
    {
        // AnyReusableHandler does not use allocators, so always return
        // the fallback allocator.
        return a;
    }
};

} // namespace asio
} // namespace boost


namespace wamp
{

namespace internal
{

template <typename H, typename F>
struct BindFallbackExecutorResult
{
    struct None{};
    using HT = typename std::decay<H>::type;
    using AE = typename boost::asio::associated_executor<HT, None>::type;
    using Missing = std::is_same<AE, None>;
    using FB = boost::asio::executor_binder<HT, F>;
    using Type = typename std::conditional<Missing::value, FB, H&&>::type;
};

template <typename H, typename F>
typename BindFallbackExecutorResult<H, F>::FB
doBindFallbackExecutor(std::true_type, H&& handler, const F& fallbackExec)
{
    return boost::asio::bind_executor(std::forward<H>(handler), fallbackExec);
}

template <typename H, typename F>
H&& doBindFallbackExecutor(std::false_type, H&& handler, const F&)
{
    return std::forward<H>(handler);
}

template <typename H, typename F>
typename BindFallbackExecutorResult<H, F>::Type
bindFallbackExecutor(H&& handler, const F& fallbackExec)
{
    using Missing = typename BindFallbackExecutorResult<H, F>::Missing;
    return doBindFallbackExecutor(Missing{}, std::forward<H>(handler),
                                  fallbackExec);
}

} // namespace internal

//------------------------------------------------------------------------------
/** Dispatches the given handler using its associated executor, passing the
    given arguments. */
//------------------------------------------------------------------------------
template <typename E, typename H, typename... Ts>
void dispatchAny(E& exec, H&& handler, Ts&&... args)
{
    boost::asio::dispatch(exec, std::bind(std::forward<H>(handler),
                                          std::forward<Ts>(args)...));
}

//------------------------------------------------------------------------------
/** Dispatches the given handler using the given fallback executor, passing the
    given arguments. */
//------------------------------------------------------------------------------
template<typename E, typename F, typename H, typename... Ts>
void dispatchVia(E& exec, const F& fallbackExec, H&& handler, Ts&&... args)
{
    boost::asio::dispatch(
        exec,
        std::bind(internal::bindFallbackExecutor(std::forward<H>(handler),
                                                 fallbackExec),
                  std::forward<Ts>(args)...));
}

//------------------------------------------------------------------------------
/** Posts the given handler using its associated executor, passing the
    given arguments. */
//------------------------------------------------------------------------------
template<typename E, typename H, typename... Ts>
void postAny(E& exec, H&& handler, Ts&&... args)
{
    boost::asio::post(exec, std::bind(std::forward<H>(handler),
                                      std::forward<Ts>(args)...));
}

//------------------------------------------------------------------------------
/** Posts the given handler using the given fallback executor, passing the
    given arguments. */
//------------------------------------------------------------------------------
template<typename E, typename F, typename H, typename... Ts>
void postVia(E& exec, const F& fallbackExec, H&& handler, Ts&&... args)
{
    boost::asio::post(
        exec,
        std::bind(internal::bindFallbackExecutor(std::forward<H>(handler),
                                                 fallbackExec),
                  std::forward<Ts>(args)...));
}

//------------------------------------------------------------------------------
/** Defers the given handler using its associated executor, passing the
    given arguments. */
//------------------------------------------------------------------------------
template<typename E, typename H, typename... Ts>
void deferAny(E& exec, H&& handler, Ts&&... args)
{
    boost::asio::defer(exec, std::bind(std::forward<H>(handler),
                                       std::forward<Ts>(args)...));
}

//------------------------------------------------------------------------------
/** Defers the given handler using the given fallback executor, passing the
    given arguments. */
//------------------------------------------------------------------------------
template<typename E, typename F, typename H, typename... Ts>
void deferVia(E& exec, const F& fallbackExec, H&& handler, Ts&&... args)
{
    boost::asio::defer(
        exec,
        std::bind(internal::bindFallbackExecutor(std::forward<H>(handler),
                                                 fallbackExec),
                  std::forward<Ts>(args)...));
}

} // namespace wamp

#endif // CPPWAMP_ANYHANDLER_HPP
