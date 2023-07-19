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
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include "traits.hpp"

// TODO: Immediate executor support for Boost >= 1.82
// https://github.com/chriskohlhoff/asio/issues/1320

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
        return !isSameType<Decay<F>, AnyReusableHandler>() &&
               !isSameType<Decay<F>, std::nullptr_t>() &&
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

    /** Asio-conformant type alias. */
    using executor_type = Executor;

    /** Default constructor. */
    AnyReusableHandler() = default;

    // NOLINTBEGIN(google-explicit-constructor)

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
          handler_(std::forward<F>(handler))
    {}

    /** Constructs an empty AnyReusableHandler. */
    AnyReusableHandler(std::nullptr_t) noexcept {};

    // NOLINTEND(google-explicit-constructor)

    /** Renders an AnyReusableHandler empty. */
    AnyReusableHandler& operator=(std::nullptr_t) noexcept
    {
        executor_ = nullptr;
        handler_ = nullptr;
        return *this;
    }

    /** Swaps contents with another AnyReusableHandler. */
    void swap(AnyReusableHandler& rhs) noexcept
    {
        // boost::asio::executor does not have member swap
        using std::swap;
        swap(executor_, rhs.executor_);

        handler_.swap(rhs.handler_);
    }

    /** Returns false iff the AnyReusableHandler is empty. */
    explicit operator bool() const noexcept
    {
        return static_cast<bool>(handler_);
    }

    /** Obtains the executor associated with this handler. */
    const Executor& get_executor() const {return executor_;}

    /** Assigns the executor to be associated with this handler. */
    void set_executor(Executor exec) {executor_ = std::move(exec);}

    /** Invokes the handler with the given arguments. */
    template <typename... Ts>
    auto operator()(Ts&&... args) const
        -> decltype(std::declval<Function>()(std::forward<Ts>(args)...))
    {
        return handler_(std::forward<Ts>(args)...);
    }

private:
    Executor executor_;
    Function handler_;

    template <typename> friend class AnyReusableHandler;
};

/** Non-member swap. @relates AnyReusableHandler */
template <typename S>
void swap(AnyReusableHandler<S>& a, AnyReusableHandler<S>& b) noexcept
{
    return a.swap(b);
}

/** Returns true if the given handler is empty. @relates AnyReusableHandler */
template <typename S>
bool operator==(const AnyReusableHandler<S>& f, std::nullptr_t) noexcept
{
    return !f;
}

/** Returns true if the given handler is empty. @relates AnyReusableHandler */
template <typename S>
bool operator==(std::nullptr_t, const AnyReusableHandler<S>& f) noexcept
{
    return !f;
}

/** Returns false if the given handler is empty. @relates AnyReusableHandler */
template <typename S>
bool operator!=(const AnyReusableHandler<S>& f, std::nullptr_t) noexcept
{
    return static_cast<bool>(f);
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

// Enable boost::asio::get_associated_allocator for AnyReusableHandler.
template <typename S, typename A>
struct associated_allocator<wamp::AnyReusableHandler<S>, A>
{
    using type = wamp::ValueTypeOf<A>;

    static type get(const wamp::AnyReusableHandler<S>&, const A& a = A{})
    {
        // AnyReusableHandler does not use allocators, so always return
        // the fallback allocator.
        return a;
    }
};

// Enable boost::asio::uses_executor for AnyReusableHandler.
template <typename S, typename E>
struct uses_executor<wamp::AnyReusableHandler<S>, E> : std::true_type
{};

} // namespace asio
} // namespace boost


namespace wamp
{

namespace internal
{

template <typename H, typename E = AnyCompletionExecutor>
struct BindFallbackExecutorResult
{
    struct None{};
    using HT = Decay<H>;
    using AE = typename boost::asio::associated_executor<HT, None>::type;
    using Missing = std::is_same<AE, None>;
    using FB = boost::asio::executor_binder<HT, E>;
    using Type = Conditional<Missing::value, FB, H&&>;
};

template <typename H, typename E>
typename BindFallbackExecutorResult<H, E>::FB
doBindFallbackExecutor(TrueType, H&& handler, const E& fallbackExec)
{
    return boost::asio::bind_executor(fallbackExec, std::forward<H>(handler));
}

template <typename H, typename E>
H&& doBindFallbackExecutor(FalseType, H&& handler, const E&)
{
    return std::forward<H>(handler);
}

template <typename H, typename E>
typename BindFallbackExecutorResult<H, E>::Type
bindFallbackExecutor(H&& handler, const E& fallbackExec)
{
    using Missing = typename BindFallbackExecutorResult<H, E>::Missing;
    return doBindFallbackExecutor(Missing{}, std::forward<H>(handler),
                                  fallbackExec);
}

template <typename THandler, typename... TArgs>
class HandlerBinder
{
public:
    template <typename F, typename... Ts>
    explicit HandlerBinder(F&& handler, Ts&&... args)
        : handler_(std::forward<F>(handler)),
          args_(std::forward<Ts>(args)...)
    {}

    void operator()()
    {
        apply(IndexSequenceFor<TArgs...>{});
    }

    template <typename E>
    typename boost::asio::associated_executor<THandler, E>::type
    executorOr(E&& e) const
    {
        return boost::asio::get_associated_executor(handler_,
                                                    std::forward<E>(e));
    }

    template <typename A>
    typename boost::asio::associated_allocator<THandler, A>::type
    allocatorOr(A&& a) const
    {
        return boost::asio::get_associated_allocator(handler_,
                                                     std::forward<A>(a));
    }

private:
    template <std::size_t... N>
    void apply(IndexSequence<N...>)
    {
        handler_(std::get<N>(std::move(args_))...);
    }

    THandler handler_;
    std::tuple<TArgs...> args_;
};

} // namespace internal
} // namespace wamp


namespace boost
{
namespace asio
{

// Enable boost::asio::get_associated_executor for AnyReusableHandler.
template <typename H, typename... Ts, typename E>
struct associated_executor<wamp::internal::HandlerBinder<H, Ts...>, E>
{
    using type = typename associated_executor<H, E>::type;

    static type get(const wamp::internal::HandlerBinder<H, Ts...>& f,
                    const E& e = E{})
    {
        return f.executorOr(e);
    }
};

// Enable boost::asio::get_associated_allocator for HandlerBinder.
template <typename H, typename... Ts, typename A>
struct associated_allocator<wamp::internal::HandlerBinder<H, Ts...>, A>
{
    using type = typename associated_allocator<H, A>::type;

    static type get(const wamp::internal::HandlerBinder<H, Ts...>& f,
                    const A& a = A{})
    {
        return f.allocatorOr(a);
    }
};

// Enable boost::asio::uses_executor for HandlerBinder.
template <typename H, typename... Ts, typename E>
struct uses_executor<wamp::internal::HandlerBinder<H, Ts...>, E>
    : uses_executor<H, E>
{};

} // namespace asio
} // namespace boost


namespace wamp
{

//------------------------------------------------------------------------------
/** Binds the given arguments to the given completion handler.
    Use this instead of std::bind to preserve the executor and allocator
    associated with the handler. */
//------------------------------------------------------------------------------
template <typename H, typename... Ts>
internal::HandlerBinder<Decay<H>, ValueTypeOf<Ts>...>
bindHandler(H&& handler, Ts&&... args)
{
    using Binder = internal::HandlerBinder<Decay<H>, ValueTypeOf<Ts>...>;
    return Binder{std::forward<H>(handler), std::forward<Ts>(args)...};
}

//------------------------------------------------------------------------------
/** Dispatches the given handler with the given arguments, while preserving
    its associated executor. */
//------------------------------------------------------------------------------
template <typename E, typename H, typename... Ts>
void dispatchAny(E& exec, H&& handler, Ts&&... args)
{
    boost::asio::dispatch(exec, bindHandler(std::forward<H>(handler),
                                            std::forward<Ts>(args)...));
}

//------------------------------------------------------------------------------
/** Posts the given handler with the given arguments, while preserving
    its associated executor. */
//------------------------------------------------------------------------------
template <typename E, typename H, typename... Ts>
void postAny(E& exec, H&& handler, Ts&&... args)
{
    boost::asio::post(exec, bindHandler(std::forward<H>(handler),
                                        std::forward<Ts>(args)...));
}

//------------------------------------------------------------------------------
/** Defers the given handler with the given arguments, while preserving
    its associated executor. */
//------------------------------------------------------------------------------
template<typename E, typename H, typename... Ts>
void deferAny(E& exec, H&& handler, Ts&&... args)
{
    boost::asio::defer(exec, bindHandler(std::forward<H>(handler),
                                         std::forward<Ts>(args)...));
}

} // namespace wamp

#endif // CPPWAMP_ANYHANDLER_HPP
