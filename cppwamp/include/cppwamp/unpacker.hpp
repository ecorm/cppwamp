/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2016, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UNPACKER_HPP
#define CPPWAMP_UNPACKER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains utilities for unpacking positional arguments passed to
           event slots and call slots. */
//------------------------------------------------------------------------------

#include <functional>
#include <tuple>
#include "api.hpp"
#include "peerdata.hpp"
#include "traits.hpp"

namespace wamp
{

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
    @see @ref UnpackedEventSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TArgs List of static types the event slot expects following the
            Event parameter. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class CPPWAMP_API EventUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit EventUnpacker(Slot slot);

    /** Dispatches the stored event slot.
        The `event.args()` positional arguments will be unpacked and passed
        to the stored event slot as additional parameters. */
    void operator()(Event event) const;

private:
    template <std::size_t... S>
    void invoke(Event&& event, IndexSequence<S...>) const;

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
CPPWAMP_API EventUnpacker<DecayedSlot<TSlot>, TArgs...>
unpackedEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around an event slot which automatically unpacks positional
    payload arguments.
    The [wamp::simpleEvent](@ref SimpleEventUnpacker::simpleEvent)
    convenience function should be used to construct instances of
    SimpleEventUnpacker.
    This class differs from EventUnpacker in that the slot type is not
    expected to take an Event as the first parameter.
    @see [wamp::simpleEvent](@ref SimpleEventUnpacker::simpleEvent)
    @see @ref UnpackedEventSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TArgs List of static types the event slot expects as arguments. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class CPPWAMP_API SimpleEventUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit SimpleEventUnpacker(Slot slot);

    /** Dispatches the stored event slot.
        The `event.args()` positional arguments will be unpacked and passed
        to the stored event slot as parameters. */
    void operator()(Event event) const;

private:
    template <std::size_t... S>
    CPPWAMP_HIDDEN void invoke(Event&& event, IndexSequence<S...>) const;

    Slot slot_;
};

//------------------------------------------------------------------------------
/** @relates SimpleEventUnpacker
    Converts an unpacked event slot into a regular slot than can be passed
    to Session::subscribe.
    This function differs from unpackedEvent in that the slot type does not
    take an Event as the first parameter.
    @see @ref UnpackedEventSlots
    @returns An SimpleEventUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the event slot expects as arguments.
    @tparam TSlot (deduced) Function type to be converted. */
//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
CPPWAMP_API SimpleEventUnpacker<DecayedSlot<TSlot>, TArgs...>
simpleEvent(TSlot&& slot);


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
class CPPWAMP_API InvocationUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /** Constructor taking a callable target. */
    explicit InvocationUnpacker(Slot slot);

    /** Dispatches the stored call slot.
        The `inv.args()` positional arguments will be unpacked and passed
        to the stored call slot as additional parameters. */
    Outcome operator()(Invocation inv) const;

private:
    template <std::size_t... S>
    CPPWAMP_HIDDEN Outcome invoke(Invocation&& inv, IndexSequence<S...>) const;

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
CPPWAMP_API InvocationUnpacker<DecayedSlot<TSlot>, TArgs...>
unpackedRpc(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around a call slot which automatically unpacks positional payload
    arguments.
    The [wamp::simpleRpc](@ref SimpleInvocationUnpacker::simpleRpc) convenience
    function should be used to construct instances of SimpleInvocationUnpacker.
    This class differs from InvocationUnpacker in that the slot type returns
    `TResult` and does not take an Invocation as the first parameter. The
    slot cannot defer the outcome of the RPC and must return a result
    immediately (or throw a wamp::Error).
    @see [wamp::simpleRpc](@ref SimpleInvocationUnpacker::simpleRpc)
    @see @ref UnpackedCallSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TArgs List of static types the call slot expects as arguments. */
//------------------------------------------------------------------------------
template <typename TSlot, typename TResult, typename... TArgs>
class CPPWAMP_API SimpleInvocationUnpacker
{
public:
    /// The function type to be wrapped.
    using Slot = TSlot;

    /// The static result type returned by the slot.
    using ResultType = TResult;

    /** Constructor taking a callable target. */
    explicit SimpleInvocationUnpacker(Slot slot);

    /** Dispatches the stored call slot.
        The `inv.args()` positional arguments will be unpacked and passed
        to the stored call slot as parameters. */
    Outcome operator()(Invocation inv) const;

private:
    template <std::size_t... S>
    Outcome invoke(TrueType, Invocation&& inv, IndexSequence<S...>) const;

    template <std::size_t... S>
    Outcome invoke(FalseType, Invocation&& inv, IndexSequence<S...>) const;

    Slot slot_;
};


//------------------------------------------------------------------------------
/** @relates SimpleInvocationUnpacker
    Converts an unpacked call slot into a regular slot than can be passed
    to Session::enroll.
    This function differs from unpackedRpc in that the slot type returns
    TResult and is not expected to take an Invocation as the first parameter.
    @see @ref UnpackedCallSlots
    @returns A SimpleInvocationUnpacker that wraps the the given slot.
    @tparam TArgs List of static types the call slot expects as arguments.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TSlot (deduced) Function type to be converted. */
//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
CPPWAMP_API SimpleInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
simpleRpc(TSlot&& slot);


//******************************************************************************
// Internal helper types
//******************************************************************************

namespace internal
{

//------------------------------------------------------------------------------
// This is caught internally by Client while dispatching RPCs and is never
// propagated through the API.
//------------------------------------------------------------------------------
struct UnpackError : public Error
{
    UnpackError() : Error(WampErrc::invalidArgument) {}
};

} // namespace internal


//******************************************************************************
// EventUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
EventUnpacker<S,A...>::EventUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
void EventUnpacker<S,A...>::operator()(Event event) const
{
    if (event.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << event.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    invoke(std::move(event), IndexSequenceFor<A...>{});
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <std::size_t... Seq>
void EventUnpacker<S,A...>::invoke(Event&& event, IndexSequence<Seq...>) const
{
    std::tuple<ValueTypeOf<A>...> args;

    try
    {
        event.convertToTuple(args);
    }
    catch (const error::Conversion& e)
    {
        throw internal::UnpackError().withArgs(e.what());
    }

    slot_(std::move(event), std::get<Seq>(std::move(args))...);
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
EventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedEvent(TSlot&& slot)
{
    return EventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}


//******************************************************************************
// SimpleEventUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
SimpleEventUnpacker<S,A...>::SimpleEventUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
void SimpleEventUnpacker<S,A...>::operator()(Event event) const
{
    if (event.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << event.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    invoke(std::move(event), IndexSequenceFor<A...>{});
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <std::size_t... Seq>
void SimpleEventUnpacker<S,A...>::invoke(Event&& event,
                                          IndexSequence<Seq...>) const
{
    std::tuple<ValueTypeOf<A>...> args;

    try
    {
        event.convertToTuple(args);
    }
    catch (const error::Conversion& e)
    {
        throw internal::UnpackError().withArgs(e.what());
    }

    slot_(std::get<Seq>(std::move(args))...);
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
SimpleEventUnpacker<DecayedSlot<TSlot>, TArgs...> simpleEvent(TSlot&& slot)
{
    return SimpleEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
SimpleEventUnpacker<DecayedSlot<TSlot>, TArgs...> basicEvent(TSlot&& slot)
{
    return SimpleEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}


//******************************************************************************
// InvocationUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
InvocationUnpacker<S,A...>::InvocationUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
Outcome InvocationUnpacker<S,A...>::operator()(Invocation inv) const
{
    if (inv.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << inv.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    return invoke(std::move(inv), IndexSequenceFor<A...>{});
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <std::size_t... Seq>
Outcome
InvocationUnpacker<S,A...>::invoke(Invocation&& inv,
                                   IndexSequence<Seq...>) const
{
    std::tuple<ValueTypeOf<A>...> args;

    try
    {
        inv.convertToTuple(args);
    }
    catch (const error::Conversion& e)
    {
        throw internal::UnpackError().withArgs(e.what());
    }

    return slot_(std::move(inv), std::get<Seq>(std::move(args))...);
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
InvocationUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedRpc(TSlot&& slot)
{
    return InvocationUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot) );
}


//******************************************************************************
// SimpleInvocationUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
SimpleInvocationUnpacker<S,R,A...>::SimpleInvocationUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
Outcome SimpleInvocationUnpacker<S,R,A...>::operator()(Invocation inv) const
{
    if (inv.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << inv.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    return invoke(std::is_void<ResultType>{}, std::move(inv),
                  IndexSequenceFor<A...>{});
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <std::size_t... Seq>
Outcome SimpleInvocationUnpacker<S,R,A...>::invoke(
    TrueType, Invocation&& inv, IndexSequence<Seq...>) const
{
    std::tuple<ValueTypeOf<A>...> args;

    try
    {
        inv.convertToTuple(args);
    }
    catch (const error::Conversion& e)
    {
        throw internal::UnpackError().withArgs(e.what());
    }

    slot_(std::get<Seq>(std::move(args))...);
    return {};
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <std::size_t... Seq>
Outcome SimpleInvocationUnpacker<S,R,A...>::invoke(
    FalseType, Invocation&& inv, IndexSequence<Seq...>) const
{
    std::tuple<ValueTypeOf<A>...> args;

    try
    {
        inv.convertToTuple(args);
    }
    catch (const error::Conversion& e)
    {
        throw internal::UnpackError().withArgs(e.what());
    }

    ResultType result = slot_(std::get<Seq>(std::move(args))...);
    return Result().withArgs(std::move(result));
}

//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
SimpleInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
simpleRpc(TSlot&& slot)
{
    return SimpleInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
        std::forward<TSlot>(slot) );
}

//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
SimpleInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicRpc(TSlot&& slot)
{
    return SimpleInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
        std::forward<TSlot>(slot) );
}

} // namespace wamp

#endif // CPPWAMP_UNPACKER_HPP
