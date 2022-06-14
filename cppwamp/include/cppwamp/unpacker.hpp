/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2016, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
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
#include <type_traits>
#include "api.hpp"
#include "peerdata.hpp"
#include "variant.hpp"
#include "./internal/integersequence.hpp"

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
CPPWAMP_API EventUnpacker<DecayedSlot<TSlot>, TArgs...>
unpackedEvent(TSlot&& slot);


//------------------------------------------------------------------------------
/** Wrapper around an event slot which automatically unpacks positional
    payload arguments.
    The [wamp::basicEvent](@ref BasicEventUnpacker::basicEvent)
    convenience function should be used to construct instances of
    BasicEventUnpacker.
    This class differs from EventUnpacker in that the slot type is not
    expected to take an Event as the first parameter.
    @see [wamp::basicEvent](@ref BasicEventUnpacker::basicEvent)
    @see @ref UnpackedEventSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TArgs List of static types the event slot expects as arguments. */
//------------------------------------------------------------------------------
template <typename TSlot, typename... TArgs>
class CPPWAMP_API BasicEventUnpacker
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
    CPPWAMP_HIDDEN void invoke(Event&& event, internal::IntegerSequence<S...>);

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
CPPWAMP_API BasicEventUnpacker<DecayedSlot<TSlot>, TArgs...>
basicEvent(TSlot&& slot);


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
    Outcome operator()(Invocation&& inv);

private:
    template <int ...S>
    CPPWAMP_HIDDEN Outcome invoke(Invocation&& inv,
                                  internal::IntegerSequence<S...>);

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
    The [wamp::basicRpc](@ref BasicInvocationUnpacker::basicRpc) convenience
    function should be used to construct instances of InvocationUnpacker.
    This class differs from InvocationUnpacker in that the slot type returns
    `TResult` and is not expected to take an Invocation as the first parameter.
    @see [wamp::basicRpc](@ref BasicInvocationUnpacker::basicRpc)
    @see @ref UnpackedCallSlots
    @tparam TSlot Function type to be wrapped.
    @tparam TResult The static result type returned by the slot (may be `void`).
    @tparam TArgs List of static types the call slot expects as arguments. */
//------------------------------------------------------------------------------
template <typename TSlot, typename TResult, typename... TArgs>
class CPPWAMP_API BasicInvocationUnpacker
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
CPPWAMP_API BasicInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicRpc(TSlot&& slot);


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
    UnpackError() : Error("wamp.error.invalid_argument") {}
};

//------------------------------------------------------------------------------
template <typename... A>
struct UnpackedArgGetter
{
    template <int N>
    static NthTypeOf<N, A...> get(const Array& args)
    {
        using TargetType = NthTypeOf<N, A...>;
        try
        {
            return args.at(N).to<TargetType>();
        }
        catch (const error::Conversion& e)
        {
            std::ostringstream oss;
            oss << "Type " << typeNameOf(args.at(N))
                << " at arg index " << N
                << " is not convertible to the RPC's target type";
            throw UnpackError().withArgs(oss.str(), e.what());
        }
    }
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
void EventUnpacker<S,A...>::operator()(Event&& event)
{
    if (event.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << event.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    invoke(std::move(event), Seq());
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int ...Seq>
void EventUnpacker<S,A...>::invoke(Event&& event,
                                    internal::IntegerSequence<Seq...>)
{
    Array args = event.args();
    using Getter = internal::UnpackedArgGetter<A...>;
    slot_(std::move(event), Getter::template get<Seq>(args)...);
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
EventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedEvent(TSlot&& slot)
{
    return EventUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot));
}


//******************************************************************************
// BasicEventUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename... A>
BasicEventUnpacker<S,A...>::BasicEventUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename... A>
void BasicEventUnpacker<S,A...>::operator()(Event&& event)
{
    if (event.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << event.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    invoke(std::move(event), Seq());
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int ...Seq>
void BasicEventUnpacker<S,A...>::invoke(Event&& event,
                                         internal::IntegerSequence<Seq...>)
{
    Array args = std::move(event).args();
    using Getter = internal::UnpackedArgGetter<A...>;
    slot_(Getter::template get<Seq>(args)...);
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
BasicEventUnpacker<DecayedSlot<TSlot>, TArgs...> basicEvent(TSlot&& slot)
{
    return BasicEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
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
Outcome InvocationUnpacker<S,A...>::operator()(Invocation&& inv)
{
    if (inv.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << inv.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    return invoke(std::move(inv), Seq());
}

//------------------------------------------------------------------------------
template <typename S, typename... A>
template <int ...Seq>
Outcome InvocationUnpacker<S,A...>::invoke(Invocation&& inv,
                                            internal::IntegerSequence<Seq...>)
{
    Array args = inv.args();
    using Getter = internal::UnpackedArgGetter<A...>;
    return slot_(std::move(inv), Getter::template get<Seq>(args)...);
}

//------------------------------------------------------------------------------
template <typename... TArgs, typename TSlot>
InvocationUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedRpc(TSlot&& slot)
{
    return InvocationUnpacker<DecayedSlot<TSlot>, TArgs...>(
        std::forward<TSlot>(slot) );
}


//******************************************************************************
// BasicInvocationUnpacker implementation
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
BasicInvocationUnpacker<S,R,A...>::BasicInvocationUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
Outcome BasicInvocationUnpacker<S,R,A...>::operator()(Invocation&& inv)
{
    if (inv.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << inv.args().size();
        throw internal::UnpackError().withArgs(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    using IsVoidResult = BoolConstant<std::is_same<ResultType, void>::value>;
    return invoke(IsVoidResult{}, std::move(inv), Seq());
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int ...Seq>
Outcome BasicInvocationUnpacker<S,R,A...>::invoke(
    TrueType, Invocation&& inv, internal::IntegerSequence<Seq...>)
{
    Array args = std::move(inv).args();
    using Getter = internal::UnpackedArgGetter<A...>;
    slot_(Getter::template get<Seq>(args)...);
    return {};
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
template <int ...Seq>
Outcome BasicInvocationUnpacker<S,R,A...>::invoke(
    FalseType, Invocation&& inv, internal::IntegerSequence<Seq...>)
{
    Array args = std::move(inv).args();
    using Getter = internal::UnpackedArgGetter<A...>;
    ResultType result = slot_(Getter::template get<Seq>(args)...);
    return Result().withArgs(std::move(result));
}

//------------------------------------------------------------------------------
template <typename TResult, typename... TArgs, typename TSlot>
BasicInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicRpc(TSlot&& slot)
{
    return BasicInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
        std::forward<TSlot>(slot) );
}

} // namespace wamp

#endif // CPPWAMP_UNPACKER_HPP
