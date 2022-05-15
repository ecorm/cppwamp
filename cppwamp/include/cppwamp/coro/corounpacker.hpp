/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CORO_COROUNPACKER_HPP
#define CPPWAMP_CORO_COROUNPACKER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains utilities for unpacking positional arguments passed to
           event slots and call slots that spawn coroutines. */
//------------------------------------------------------------------------------

#include <memory>
#include <boost/asio/spawn.hpp>
#include "../session.hpp"
#include "../unpacker.hpp"
#include "../internal/integersequence.hpp"

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
            `void function(Event, TArgs..., boost::asio::yield_context)`.
    @tparam TArgs List of static types the event slot expects following the
            Event parameter and preceding the `boost::asio::yield_context`
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
    void operator()(Event&& event);

private:
    using Yield = boost::asio::yield_context;

    template <int ...S>
    void invoke(Event&& event, internal::IntegerSequence<S...>);

    std::shared_ptr<Slot> slot_;
};

//------------------------------------------------------------------------------
/** @relates CoroEventUnpacker
    Converts an unpacked event slot into a regular slot than can be passed
    to Session::subscribe.
    The slot will be executed within the context of a coroutine and will be
    given a `boost::asio::yield_context` as the last call argument.
    @see @ref UnpackedCoroutineEventSlots
    @returns An CoroEventUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the event slot expects following the
            Event parameter, and preceding the
            `boost::asio::yield_context` parameter.
    @tparam TSlot (deduced) Function type to be converted. Must have the signature
            `void function(Event, TArgs..., boost::asio::yield_context)`. */
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedCoroEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around an event slot which automatically unpacks positional
    payload arguments.
    The [wamp::basicCoroEvent](@ref BasicCoroEventUnpacker::basicCoroEvent)
    convenience function should be used to construct instances of
    BasicCoroEventUnpacker.
    This class differs from CoroEventUnpacker in that the slot type is not
    expected to take an Event as the first parameter.
    @see [wamp::basicCoroEvent](@ref BasicCoroEventUnpacker::basicCoroEvent)
    @see @ref BasicCoroutineEventSlots
    @tparam TSlot Function type to be wrapped. Must have the signature
            `void function(TArgs..., boost::asio::yield_context)`.
    @tparam TArgs List of static types the event slot expects as arguments
            preceding the `boost::asio::yield_context` parameter. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class BasicCoroEventUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit BasicCoroEventUnpacker(Slot slot);

    /** Spawns a new coroutine and executes the stored event slot.
        The coroutine will be spawned using `event.executor()`.
        The `event.args()` positional arguments will be unpacked and passed
        to the stored event slot as parameters. */
    void operator()(Event&& event);

private:
    using Yield = boost::asio::yield_context;

    template <int ...S>
    void invoke(Event&& event, internal::IntegerSequence<S...>);

    std::shared_ptr<Slot> slot_;
};

//------------------------------------------------------------------------------
/** @relates BasicCoroEventUnpacker
    Converts an unpacked event slot into a regular slot than can be passed
    to Session::subscribe.
    This function differs from `unpackedCoroEvent` in that the slot type is not
    expected to take an Event as the first parameter.
    @see @ref BasicCoroutineEventSlots
    @returns An BasicCoroEventUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the event slot expects as arguments,
                  preceding the `boost::asio::yield_context` parameter.
    @tparam TSlot (deduced) Function type to be converted. Must have the
            signature `void function(TArgs..., boost::asio::yield_context)`.*/
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
BasicCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>
basicCoroEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around a call coroutine slot which automatically unpacks positional
    payload arguments.
    The [wamp::unpackedCoroRpc](@ref CoroInvocationUnpacker::unpackedCoroRpc)
    convenience function should be used to construct instances of
    CoroInvocationUnpacker.
    @see [wamp::unpackedCoroRpc](@ref CoroInvocationUnpacker::unpackedCoroRpc)
    @see @ref UnpackedCoroutineCallSlots
    @tparam TSlot Function type to be wrapped. Must have the signature
            `void function(Invocation, TArgs..., boost::asio::yield_context)`.
    @tparam TArgs List of static types the call slot expects following the
            Invocation parameter, and preceding the boost::asio::yield_context
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
    Outcome operator()(Invocation&& inv);

private:
    using Yield = boost::asio::yield_context;

    template <int ...S>
    void invoke(Invocation&& inv, internal::IntegerSequence<S...>);

    std::shared_ptr<Slot> slot_;
};

//------------------------------------------------------------------------------
/** @relates CoroInvocationUnpacker
    Converts an unpacked call slot into a regular slot than can be passed
    to Session::enroll.
    @see @ref UnpackedCoroutineCallSlots
    @returns A CoroInvocationUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the call slot expects following the
                  Invocation parameter, and preceding the
                  `boost::asio::yield_context` parameter.
    @tparam TSlot (deduced) Function type to be converted. Must have the signature
            `Outcome function(Invocation, TArgs..., boost::asio::yield_context)`.*/
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CoroInvocationUnpacker<DecayedSlot<TSlot>, TArgs...>
unpackedCoroRpc(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around a call slot which automatically unpacks positional payload
    arguments.
    The [wamp::basicCoroRpc](@ref BasicCoroInvocationUnpacker::basicCoroRpc)
    convenience function should be used to construct instances of
    BasicCoroInvocationUnpacker.
    This class differs from CoroInvocationUnpacker in that the slot type returns
    `TResult` and is not expected to take an Invocation as the first parameter.
    @see [wamp::basicCoroRpc](@ref BasicCoroInvocationUnpacker::basicCoroRpc)
    @see @ref BasicCoroutineCallSlots
    @tparam TSlot Function type to be wrapped. Must have the signature
            `TResult function(TArgs..., boost::asio::yield_context)`.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TArgs List of static types the call slot expects as arguments,
            preceding the `boost::asio::yield_context` argument. */
//------------------------------------------------------------------------------
template <typename TSlot, typename TResult, typename... TArgs>
class BasicCoroInvocationUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /// The static result type returned by the slot.
    using ResultType = TResult;

    /** Constructor taking a callable target. */
    explicit BasicCoroInvocationUnpacker(Slot slot);

    /** Spawns a new coroutine and executes the stored call slot.
        The coroutine will be spawned using `inv.executor()`.
        The `inv.args()` positional arguments will be unpacked and passed
        to the stored call slot as additional parameters. */
    Outcome operator()(Invocation&& inv);

private:
    using Yield = boost::asio::yield_context;

    template <int ...S>
    void invoke(TrueType, Invocation&& inv, internal::IntegerSequence<S...>);

    template <int ...S>
    void invoke(FalseType, Invocation&& inv, internal::IntegerSequence<S...>);

    std::shared_ptr<Slot> slot_;
};

//------------------------------------------------------------------------------
/** @relates BasicCoroInvocationUnpacker
    Converts an unpacked call slot into a regular slot than can be passed
    to Session::enroll.
    This function differs from `unpackedCoroRpc` in that the slot type returns
    TResult and is not expected to take an Invocation as the first parameter.
    @see @ref BasicCoroutineCallSlots
    @returns A BasicCoroInvocationUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the call slot expects as arguments,
            preceding the `boost::asio::yield_context` argument.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TSlot (deduced) Function type to be converted. Must have the signature
            `TResult function(TArgs..., boost::asio::yield_context)`.*/
//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
BasicCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicCoroRpc(TSlot&& slot);


//******************************************************************************
// Internal helper types
//******************************************************************************

namespace internal
{

//------------------------------------------------------------------------------
// This is caught internally by Client while dispatching RPCs and is never
// propagated through the API.
//------------------------------------------------------------------------------
struct UnpackCoroError : public Error
{
    UnpackCoroError() : Error("wamp.error.invalid_argument") {}
};

//------------------------------------------------------------------------------
template <typename... A>
struct UnpackedCoroArgGetter
{
    template <int N>
    static NthTypeOf<N, A...> get(const Array& args)
    {
        using TargetType = NthTypeOf<N, A...>;
        try
        {
            return args.at(N).to<TargetType>();
        }
        catch(const error::Conversion& e)
        {
            std::ostringstream oss;
            oss << "Type " << typeNameOf(args.at(N))
                << " at arg index " << N
                << " is not convertible to the RPC's target type";
            throw UnpackCoroError().withArgs(oss.str(), e.what());
        }
    }
};

} // namespace internal


//******************************************************************************
// CoroEventUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
CoroEventUnpacker<S,A...>::CoroEventUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
void CoroEventUnpacker<S,A...>::operator()(Event&& event)
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
template <int ...Seq>
void CoroEventUnpacker<S,A...>::invoke(Event&& event,
                                       internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(event.executor(), [slot, event](Yield yield)
    {
        Array args = event.args();
        using Getter = internal::UnpackedCoroArgGetter<A...>;
        try
        {
            (*slot)(std::move(event), Getter::template get<Seq>(args)...,
            yield);
        }
        catch (const error::BadType& e)
        {
            /*  Do nothing. This is to prevent the publisher crashing
            subscribers when it passes Variant objects having
            incorrect schema. */
        }
    });
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedCoroEvent(TSlot&& slot)
{
    return CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}


//******************************************************************************
// BasicCoroEventUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
BasicCoroEventUnpacker<S,A...>::BasicCoroEventUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
void BasicCoroEventUnpacker<S,A...>::operator()(Event&& event)
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
template <int ...Seq>
void BasicCoroEventUnpacker<S,A...>::invoke(Event&& event,
                                            internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    Array args = std::move(event).args();

    boost::asio::spawn(event.executor(), [slot, args](Yield yield)
    {
        using Getter = internal::UnpackedCoroArgGetter<A...>;
        try
        {
           (*slot)(Getter::template get<Seq>(args)..., yield);
        }
        catch (const error::BadType& e)
        {
           /* Do nothing. This is to prevent the publisher crashing
              subscribers when it passes Variant objects having
              incorrect schema. */
        }
    });
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
BasicCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>
basicCoroEvent(TSlot&& slot)
{
    return BasicCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}


//******************************************************************************
// CoroInvocationUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
CoroInvocationUnpacker<S,A...>::CoroInvocationUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
Outcome CoroInvocationUnpacker<S,A...>::operator()(Invocation&& inv)
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

    return Outcome::deferred();
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int ...Seq>
void CoroInvocationUnpacker<S,A...>::invoke(Invocation&& inv,
                                             internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(inv.executor(), [slot, inv](Yield yield)
    {
        try
        {
            Array args = inv.args();
            using Getter = internal::UnpackedCoroArgGetter<A...>;
            Outcome outcome = (*slot)(std::move(inv),
            Getter::template get<Seq>(args)...,
            yield);

            switch (outcome.type())
            {
            case Outcome::Type::deferred:
            // Do nothing
            break;

            case Outcome::Type::result:
            inv.yield(std::move(outcome).asResult());
            break;

            case Outcome::Type::error:
            inv.yield(std::move(outcome).asError());
            break;

            default:
            assert(false && "Unexpected wamp::Outcome::Type enumerator");
        }

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
    });
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
// BasicCoroInvocationUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
BasicCoroInvocationUnpacker<S,R,A...>::BasicCoroInvocationUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
Outcome BasicCoroInvocationUnpacker<S,R,A...>::operator()(Invocation&& inv)
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
    using IsVoidResult = BoolConstant<std::is_same<ResultType, void>::value>;
    invoke(IsVoidResult{}, std::move(inv), Seq());

    return Outcome::deferred();
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int ...Seq>
void BasicCoroInvocationUnpacker<S,R,A...>::invoke(
    TrueType, Invocation&& inv, internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(inv.executor(), [slot, inv](Yield yield)
    {
        try
        {
            Array args = std::move(inv).args();
            using Getter = internal::UnpackedCoroArgGetter<A...>;
            (*slot)(Getter::template get<Seq>(args)..., yield);
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
    });
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int ...Seq>
void BasicCoroInvocationUnpacker<S,R,A...>::invoke(
    FalseType, Invocation&& inv, internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(inv.executor(), [slot, inv](Yield yield)
    {
        try
        {
            Array args = std::move(inv).args();
            using Getter = internal::UnpackedCoroArgGetter<A...>;
            ResultType result = (*slot)(Getter::template get<Seq>(args)...,
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
    });
}

//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
BasicCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicCoroRpc(TSlot&& slot)
{
    return BasicCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
        std::forward<TSlot>(slot) );
}

} // namespace wamp

#endif // CPPWAMP_CORO_COROUNPACKER_HPP
