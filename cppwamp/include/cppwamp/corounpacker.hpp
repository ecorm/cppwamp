/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COROUNPACKER_HPP
#define CPPWAMP_COROUNPACKER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains utilities for unpacking positional arguments passed to
           event slots and call slots that spawn coroutines. */
//------------------------------------------------------------------------------

#include <exception>
#include <boost/version.hpp>
#include "spawn.hpp"
#include "unpacker.hpp"
#include "internal/callee.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Wrapper around an event coroutine slot which automatically unpacks
    positional payload arguments.
    The [wamp::unpackedCoroEvent](@ref CoroEventUnpacker::unpackedCoroEvent)
    convenience function should be used to construct instances of
    CoroEventUnpacker.
    @see [wamp::unpackedCoroEvent](@ref CoroEventUnpacker::unpackedCoroEvent)
    @see @ref UnpackedCoroutineEventSlots
    @tparam TSlot Function type to be wrapped. Must have the signature
            `void function(Event, TArgs..., wamp::YieldContext)`.
    @tparam TArgs List of static types the event slot expects following the
            Event parameter and preceding the `wamp::YieldContext`
            parameter. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class CoroEventUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit CoroEventUnpacker(Slot slot);

    /** Spawns a new coroutine and executes the stored event slot.
        The coroutine will be spawned using `event.executor()`.
        The `event.args()` positional arguments will be unpacked and passed
        to the stored event slot as additional parameters. */
    void operator()(Event event) const;

private:
    template <int... Seq> struct Spawned;

    template <int... S>
    void invoke(Event&& event, internal::IntegerSequence<S...>) const;

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates CoroEventUnpacker
    Converts an unpacked event slot into a regular slot than can be passed
    to Session::subscribe.
    The slot will be executed within the context of a coroutine and will be
    given a `wamp::YieldContext` as the last call argument.
    @see @ref UnpackedCoroutineEventSlots
    @returns An CoroEventUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the event slot expects following the
            Event parameter, and preceding the
            `wamp::YieldContext` parameter.
    @tparam TSlot (deduced) Function type to be converted. Must have the signature
            `void function(Event, TArgs..., wamp::YieldContext)`. */
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedCoroEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around an event slot which automatically unpacks positional
    payload arguments.
    The [wamp::simpleCoroEvent](@ref SimpleCoroEventUnpacker::simpleCoroEvent)
    convenience function should be used to construct instances of
    SimpleCoroEventUnpacker.
    This class differs from CoroEventUnpacker in that the slot type is not
    expected to take an Event as the first parameter.
    @see [wamp::simpleCoroEvent](@ref SimpleCoroEventUnpacker::simpleCoroEvent)
    @see @ref SimpleCoroutineEventSlots
    @tparam TSlot Function type to be wrapped. Must have the signature
            `void function(TArgs..., wamp::YieldContext)`.
    @tparam TArgs List of static types the event slot expects as arguments
            preceding the `wamp::YieldContext` parameter. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class SimpleCoroEventUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit SimpleCoroEventUnpacker(Slot slot);

    /** Spawns a new coroutine and executes the stored event slot.
        The coroutine will be spawned using `event.executor()`.
        The `event.args()` positional arguments will be unpacked and passed
        to the stored event slot as parameters. */
    void operator()(Event event) const;

private:
    template <int... Seq> struct Spawned;

    template <int... S>
    void invoke(Event&& event, internal::IntegerSequence<S...>) const;

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates SimpleCoroEventUnpacker
    Converts an unpacked event slot into a regular slot than can be passed
    to Session::subscribe.
    This function differs from `unpackedCoroEvent` in that the slot type is not
    expected to take an Event as the first parameter.
    @see @ref SimpleCoroutineEventSlots
    @returns An SimpleCoroEventUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the event slot expects as arguments,
                  preceding the `wamp::YieldContext` parameter.
    @tparam TSlot (deduced) Function type to be converted. Must have the
            signature `void function(TArgs..., wamp::YieldContext)`.*/
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
SimpleCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>
simpleCoroEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around a call coroutine slot which automatically unpacks positional
    payload arguments.
    The [wamp::unpackedCoroRpc](@ref CoroInvocationUnpacker::unpackedCoroRpc)
    convenience function should be used to construct instances of
    CoroInvocationUnpacker.
    @see [wamp::unpackedCoroRpc](@ref CoroInvocationUnpacker::unpackedCoroRpc)
    @see @ref UnpackedCoroutineCallSlots
    @tparam TSlot Function type to be wrapped. Must have the signature
            `void function(Invocation, TArgs..., wamp::YieldContext)`.
    @tparam TArgs List of static types the call slot expects following the
            Invocation parameter, and preceding the wamp::YieldContext
            parameter. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class CoroInvocationUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit CoroInvocationUnpacker(Slot slot);

    /** Spawns a new coroutine and executes the stored call slot.
        The coroutine will be spawned using `inv.executor()`.
        The `inv.args()` positional arguments will be unpacked and passed
        to the stored call slot as additional parameters. */
    Outcome operator()(Invocation inv) const;

private:
    template <int... Seq> class Spawned;

    template <int... S>
    void invoke(Invocation&& inv, internal::IntegerSequence<S...>) const;

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates CoroInvocationUnpacker
    Converts an unpacked call slot into a regular slot than can be passed
    to Session::enroll.
    @see @ref UnpackedCoroutineCallSlots
    @returns A CoroInvocationUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the call slot expects following the
                  Invocation parameter, and preceding the
                  `wamp::YieldContext` parameter.
    @tparam TSlot (deduced) Function type to be converted. Must have the signature
            `Outcome function(Invocation, TArgs..., wamp::YieldContext)`.*/
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CoroInvocationUnpacker<DecayedSlot<TSlot>, TArgs...>
unpackedCoroRpc(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around a call slot which automatically unpacks positional payload
    arguments.
    The [wamp::simpleCoroRpc](@ref SimpleCoroInvocationUnpacker::simpleCoroRpc)
    convenience function should be used to construct instances of
    SimpleCoroInvocationUnpacker.
    This class differs from CoroInvocationUnpacker in that the slot type returns
    `TResult` and does not take an Invocation as the first parameter. The
    slot cannot defer the outcome of the RPC and must return a result
    immediately (or throw a wamp::Error).
    @see [wamp::simpleCoroRpc](@ref SimpleCoroInvocationUnpacker::simpleCoroRpc)
    @see @ref SimpleCoroutineCallSlots
    @tparam TSlot Function type to be wrapped. Must have the signature
            `TResult function(TArgs..., wamp::YieldContext)`.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TArgs List of static types the call slot expects as arguments,
            preceding the `wamp::YieldContext` argument. */
//------------------------------------------------------------------------------
template <typename TSlot, typename TResult, typename... TArgs>
class SimpleCoroInvocationUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /// The static result type returned by the slot.
    using ResultType = TResult;

    /** Constructor taking a callable target. */
    explicit SimpleCoroInvocationUnpacker(Slot slot);

    /** Spawns a new coroutine and executes the stored call slot.
        The coroutine will be spawned using `inv.executor()`.
        The `inv.args()` positional arguments will be unpacked and passed
        to the stored call slot as additional parameters. */
    Outcome operator()(Invocation inv) const;

private:
    template <int... Seq> struct SpawnedWithVoid;
    template <int... Seq> struct SpawnedWithResult;

    template <int... S>
    void invoke(TrueType, Invocation&& inv,
                internal::IntegerSequence<S...>) const;

    template <int... S>
    void invoke(FalseType, Invocation&& inv,
                internal::IntegerSequence<S...>) const;

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates SimpleCoroInvocationUnpacker
    Converts an unpacked call slot into a regular slot than can be passed
    to Session::enroll.
    This function differs from `unpackedCoroRpc` in that the slot type returns
    TResult and is not expected to take an Invocation as the first parameter.
    @see @ref SimpleCoroutineCallSlots
    @returns A SimpleCoroInvocationUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the call slot expects as arguments,
            preceding the `wamp::YieldContext` argument.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TSlot (deduced) Function type to be converted. Must have the signature
            `TResult function(TArgs..., wamp::YieldContext)`.*/
//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
SimpleCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
simpleCoroRpc(TSlot&& slot);


//******************************************************************************
// Internal helpers
//******************************************************************************

namespace internal
{

//------------------------------------------------------------------------------
// This is caught internally by Client while dispatching RPCs and is never
// propagated through the API.
//------------------------------------------------------------------------------
struct UnpackCoroError : public Error
{
    UnpackCoroError() : Error(SessionErrc::invalidArgument) {}
};

//------------------------------------------------------------------------------
template <typename E>
struct UnpackedSpawner
{
    template <typename F>
    static void spawn(E& executor, F&& function)
    {
        spawn(executor, std::forward<F>(function));
    }
};

template <>
struct UnpackedSpawner<AnyCompletionExecutor>
{
    template <typename F>
    static void spawn(AnyCompletionExecutor& executor, F&& function)
    {
        spawnCompletionHandler(executor, std::forward<F>(function));
    }
};

template <typename E, typename F>
void unpackedSpawn(E& executor, F&& function)
{
#ifdef CPPWAMP_USE_COMPLETION_YIELD_CONTEXT
    spawn(executor, std::forward<F>(function), propagating);
#else
    UnpackedSpawner<E>::spawn(executor, std::forward<F>(function));
#endif
}

} // namespace internal


//******************************************************************************
// CoroEventUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
CoroEventUnpacker<S,A...>::CoroEventUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
void CoroEventUnpacker<S,A...>::operator()(Event event) const
{
    if (event.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << event.args().size();
        throw internal::UnpackCoroError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    invoke(std::move(event), Seq());
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int... Seq>
struct CoroEventUnpacker<S,A...>::Spawned
{
    Slot slot;
    Event event;

    template <typename TYieldContext>
    void operator()(TYieldContext yield)
    {
        std::tuple<ValueTypeOf<A>...> args;
        try
        {
            event.convertToTuple(args);
            slot(std::move(event), std::get<Seq>(std::move(args))...,
                 yield);
        }
        catch (const error::BadType&)
        {
            /*  Do nothing. This is to prevent the publisher crashing
                subscribers when it passes Variant objects having
                incorrect schema. */
        }
    }
};

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int... Seq>
void CoroEventUnpacker<S,A...>::invoke(Event&& event,
                                       internal::IntegerSequence<Seq...>) const
{
    auto ex = boost::asio::get_associated_executor(slot_, event.executor());
    internal::unpackedSpawn(ex, Spawned<Seq...>{slot_, std::move(event)});
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedCoroEvent(TSlot&& slot)
{
    return CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}


//******************************************************************************
// SimpleCoroEventUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
SimpleCoroEventUnpacker<S,A...>::SimpleCoroEventUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
void SimpleCoroEventUnpacker<S,A...>::operator()(Event event) const
{
    if (event.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << event.args().size();
        throw internal::UnpackCoroError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    invoke(std::move(event), Seq());
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int... Seq>
struct SimpleCoroEventUnpacker<S,A...>::Spawned
{
    Slot slot;
    Event event;

    template <typename TYieldContext>
    void operator()(TYieldContext yield)
    {
        std::tuple<ValueTypeOf<A>...> args;
        try
        {
            event.convertToTuple(args);
            slot(std::get<Seq>(std::move(args))..., yield);
        }
        catch (const error::BadType&)
        {
            /* Do nothing. This is to prevent the publisher crashing
                   subscribers when it passes Variant objects having
                   incorrect schema. */
        }
    }
};

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int... Seq>
void
SimpleCoroEventUnpacker<S,A...>::invoke(Event&& event,
                                       internal::IntegerSequence<Seq...>) const
{
    auto ex = boost::asio::get_associated_executor(slot_, event.executor());
    internal::unpackedSpawn(ex, Spawned<Seq...>{slot_, std::move(event)});
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
SimpleCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>
simpleCoroEvent(TSlot&& slot)
{
    return SimpleCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
SimpleCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>
basicCoroEvent(TSlot&& slot)
{
    return SimpleCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}


//******************************************************************************
// CoroInvocationUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
CoroInvocationUnpacker<S,A...>::CoroInvocationUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
Outcome CoroInvocationUnpacker<S,A...>::operator()(Invocation inv) const
{
    if (inv.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << inv.args().size();
        throw internal::UnpackCoroError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    invoke(std::move(inv), Seq());

    return deferment;
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int... Seq>
class CoroInvocationUnpacker<S,A...>::Spawned
{
public:
    Spawned(const Slot& slot, Invocation&& inv)
        : slot_(slot),
        calleePtr_(inv.callee_),
        reqId_(inv.requestId()),
        inv_(std::move(inv))
    {}

    template <typename TYieldContext>
    void operator()(TYieldContext yield)
    {
        try
        {
            std::tuple<ValueTypeOf<A>...> args;
            inv_.convertToTuple(args);
            Outcome outcome = slot_(std::move(inv_),
                                    std::get<Seq>(std::move(args))...,
                                    yield);

            switch (outcome.type())
            {
            case Outcome::Type::deferred:
                // Do nothing
                break;

            case Outcome::Type::result:
                safeYield(std::move(outcome).asResult());
                break;

            case Outcome::Type::error:
                safeYield(std::move(outcome).asError());
                break;

            default:
                assert(false && "Unexpected wamp::Outcome::Type enumerator");
            }

        }
        catch (Error e)
        {
            safeYield(std::move(e));
        }
        catch (const error::BadType& e)
        {
            // Forward Variant conversion exceptions as ERROR messages.
            safeYield(Error(e));
        }
    }

private:
    void safeYield(Result&& result)
    {
        auto callee = calleePtr_.lock();
        if (callee)
            callee->safeYield(reqId_, std::move(result));
    }

    void safeYield(Error&& error)
    {
        auto callee = calleePtr_.lock();
        if (callee)
            callee->safeYield(reqId_, std::move(error));
    }

    Slot slot_;
    std::weak_ptr<internal::Callee> calleePtr_;
    RequestId reqId_;
    Invocation inv_;
};


//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int... Seq>
void
CoroInvocationUnpacker<S,A...>::invoke(Invocation&& inv,
                                       internal::IntegerSequence<Seq...>) const
{
    auto ex = boost::asio::get_associated_executor(slot_, inv.executor());
    internal::unpackedSpawn(ex, Spawned<Seq...>{slot_, std::move(inv)});
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CoroInvocationUnpacker<DecayedSlot<TSlot>, TArgs...>
unpackedCoroRpc(TSlot&& slot)
{
    return CoroInvocationUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot) );
}

//******************************************************************************
// SimpleCoroInvocationUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
SimpleCoroInvocationUnpacker<S,R,A...>::SimpleCoroInvocationUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
Outcome
SimpleCoroInvocationUnpacker<S,R,A...>::operator()(Invocation inv) const
{
    if (inv.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << inv.args().size();
        throw internal::UnpackCoroError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    using IsVoidResult = MetaBool<std::is_same<ResultType, void>::value>;
    invoke(IsVoidResult{}, std::move(inv), Seq());

    return deferment;
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int... Seq>
struct SimpleCoroInvocationUnpacker<S,R,A...>::SpawnedWithVoid
{
    Slot slot;
    Invocation inv;

    template <typename TYieldContext>
    void operator()(TYieldContext yield)
    {
        try
        {
            std::tuple<ValueTypeOf<A>...> args;
            inv.convertToTuple(args);
            slot(std::get<Seq>(std::move(args))..., yield);
            inv.yield();
        }
        catch (const Error& e)
        {
            inv.yield(e);
        }
        catch (const error::BadType& e)
        {
            // Forward Variant conversion exceptions as ERROR messages.
            inv.yield(Error(e));
        }
    }
};

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int... Seq>
struct SimpleCoroInvocationUnpacker<S,R,A...>::SpawnedWithResult
{
    Slot slot;
    Invocation inv;

    template <typename TYieldContext>
    void operator()(TYieldContext yield)
    {
        try
        {
            std::tuple<ValueTypeOf<A>...> args;
            inv.convertToTuple(args);
            ResultType result = slot(std::get<Seq>(std::move(args))...,
                                     yield);
            inv.yield(Result().withArgs(std::move(result)));
        }
        catch (const Error& e)
        {
            inv.yield(e);
        }
        catch (const error::BadType& e)
        {
            // Forward Variant conversion exceptions as ERROR messages.
            inv.yield(Error(e));
        }
    }
};

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int... Seq>
void SimpleCoroInvocationUnpacker<S,R,A...>::invoke(
    TrueType, Invocation&& inv, internal::IntegerSequence<Seq...>) const
{
    auto ex = boost::asio::get_associated_executor(slot_, inv.executor());
    internal::unpackedSpawn(ex, SpawnedWithVoid<Seq...>{slot_, std::move(inv)});
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int... Seq>
void SimpleCoroInvocationUnpacker<S,R,A...>::invoke(
    FalseType, Invocation&& inv, internal::IntegerSequence<Seq...>) const
{
    using std::move;
    auto ex = boost::asio::get_associated_executor(slot_, inv.executor());
    internal::unpackedSpawn(ex, SpawnedWithResult<Seq...>{slot_, move(inv)});
}

//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
SimpleCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
simpleCoroRpc(TSlot&& slot)
{
    return SimpleCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
        std::forward<TSlot>(slot) );
}

//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
SimpleCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicCoroRpc(TSlot&& slot)
{
    return SimpleCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
        std::forward<TSlot>(slot) );
}

} // namespace wamp

#endif // CPPWAMP_COROUNPACKER_HPP
