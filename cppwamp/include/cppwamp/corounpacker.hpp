/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COROUNPACKER_HPP
#define CPPWAMP_COROUNPACKER_HPP

//------------------------------------------------------------------------------
/** @file
    Contains utilities for unpacking positional arguments passed to
    event slots and call slots that spawn coroutines. */
//------------------------------------------------------------------------------

#include <memory>
#include <boost/asio/spawn.hpp>
#include "unpacker.hpp"

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
        The coroutine will be spawned using `event.iosvc()`.
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
        The coroutine will be spawned using `event.iosvc()`.
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
        The coroutine will be spawned using `inv.iosvc()`.
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
        The coroutine will be spawned using `inv.iosvc()`.
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


} // namespace wamp

#include "./internal/corounpacker.ipp"

#endif // CPPWAMP_COROUNPACKER_HPP
