/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UNPACKER_HPP
#define CPPWAMP_UNPACKER_HPP

//------------------------------------------------------------------------------
/** @file
    Contains utilities for unpacking positional arguments passed to
    event slots and call slots. */
//------------------------------------------------------------------------------

#include <functional>
#include <tuple>
#include <type_traits>
#include "sessiondata.hpp"
#include "variant.hpp"
#include "./internal/integersequence.hpp"

namespace wamp
{

namespace internal
{

struct UnpackError
{
    explicit UnpackError(std::string reason) : reason(std::move(reason)) {}
    std::string reason;
};

} // namespace internal


//------------------------------------------------------------------------------
/** Metafunction that removes const/reference decorations off a slot type */
//------------------------------------------------------------------------------
template <typename TSlot>
using DecayedSlot = typename std::decay<TSlot>::type;


//------------------------------------------------------------------------------
/** Wrapper around an event slot which automatically unpacks positional
    payload arguments.
    The [wamp::unpackedEvent](@ref EventUnpacker::unpackedEvent) convenience
    function should be used to construct instances of EventUnpacker.
    @see [wamp::unpackedEvent](@ref EventUnpacker::unpackedEvent)
    @see @ref UnpackedCallSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TArgs List of static types the event slot expects following the
            Event parameter. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class EventUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit EventUnpacker(Slot slot);

    /** Dispatches the stored event slot.
        The `event.args()` positional arguments will be unpacked and passed
        to the stored event slot as additional parameters. */
    void operator()(Event&& event);

private:
    template <int ...S>
    void invoke(Event&& event, internal::IntegerSequence<S...>);

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates EventUnpacker
    Converts an unpacked event slot into a regular slot than can be passed
    to Session::subscribe.
    @see @ref UnpackedEventSlots
    @returns An EventUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the event slot expects following the
                  Event parameter.
    @tparam TSlot (deduced) Function type to be converted. */
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
EventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around an event slot which automatically unpacks positional
    payload arguments.
    The [wamp::basicEvent](@ref BasicEventUnpacker::basicEvent)
    convenience function should be used to construct instances of
    BasicEventUnpacker.
    This class differs from EventUnpacker in that the slot type is not
    expected to take an Event as the first parameter.
    @see [wamp::basicEvent](@ref BasicEventUnpacker::basicEvent)
    @see @ref UnpackedCallSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TArgs List of static types the event slot expects as arguments. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class BasicEventUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit BasicEventUnpacker(Slot slot);

    /** Dispatches the stored event slot.
        The `event.args()` positional arguments will be unpacked and passed
        to the stored event slot as parameters. */
    void operator()(Event&& event);

private:
    template <int ...S>
    void invoke(Event&& event, internal::IntegerSequence<S...>);

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates BasicEventUnpacker
    Converts an unpacked event slot into a regular slot than can be passed
    to Session::subscribe.
    This function differs from unpackedEvent in that the slot type is not
    expected to take an Event as the first parameter.
    @see @ref UnpackedEventSlots
    @returns An BasicEventUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the event slot expects as arguments.
    @tparam TSlot (deduced) Function type to be converted. */
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
BasicEventUnpacker<DecayedSlot<TSlot>, TArgs...> basicEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around a call slot which automatically unpacks positional payload
    arguments.
    The [wamp::unpackedRpc](@ref InvocationUnpacker::unpackedRpc) convenience
    function should be used to construct instances of InvocationUnpacker.
    @see [wamp::unpackedRpc](@ref InvocationUnpacker::unpackedRpc)
    @see @ref UnpackedCallSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TArgs List of static types the call slot expects following the
            Invocation parameter. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class InvocationUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit InvocationUnpacker(Slot slot);

    /** Dispatches the stored call slot.
        The `inv.args()` positional arguments will be unpacked and passed
        to the stored call slot as additional parameters. */
    Outcome operator()(Invocation&& inv);

private:
    template <int ...S>
    Outcome invoke(Invocation&& inv, internal::IntegerSequence<S...>);

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates InvocationUnpacker
    Converts an unpacked call slot into a regular slot than can be passed
    to Session::enroll.
    @see @ref UnpackedCallSlots
    @returns An InvocationUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the call slot expects following the
                  Invocation parameter.
    @tparam TSlot (deduced) Function type to be converted. */
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
InvocationUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedRpc(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around a call slot which automatically unpacks positional payload
    arguments.
    The [wamp::basicRpc](@ref BasicInvocationUnpacker::basicRpc) convenience
    function should be used to construct instances of InvocationUnpacker.
    This class differs from InvocationUnpacker in that the slot type returns
    void and is not expected to take an Invocation as the first parameter.
    @see [wamp::basicRpc](@ref BasicInvocationUnpacker::basicRpc)
    @see @ref UnpackedCallSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TArgs List of static types the call slot expects as arguments. */
//------------------------------------------------------------------------------
template <typename TSlot, typename TResult, typename... TArgs>
class BasicInvocationUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /// The static result type returned by the slot.
    using ResultType = TResult;

    /** Constructor taking a callable target. */
    explicit BasicInvocationUnpacker(Slot slot);

    /** Dispatches the stored call slot.
        The `inv.args()` positional arguments will be unpacked and passed
        to the stored call slot as parameters. */
    Outcome operator()(Invocation&& inv);

private:
    template <int ...S>
    Outcome invoke(TrueType, Invocation&& inv,
                   internal::IntegerSequence<S...>);

    template <int ...S>
    Outcome invoke(FalseType, Invocation&& inv,
                   internal::IntegerSequence<S...>);

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates BasicInvocationUnpacker
    Converts an unpacked call slot into a regular slot than can be passed
    to Session::enroll.
    This function differs from unpackedRpc in that the slot type returns
    void and is not expected to take an Invocation as the first parameter.
    @see @ref UnpackedCallSlots
    @returns A BasicInvocationUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the call slot expects as arguments.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TSlot (deduced) Function type to be converted. */
//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
BasicInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicRpc(TSlot&& slot);


} // namespace wamp

#include "./internal/unpacker.ipp"

#endif // CPPWAMP_UNPACKER_HPP
