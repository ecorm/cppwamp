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
#include "sessiondata.hpp"
#include "variant.hpp"

namespace wamp
{

namespace internal
{

template<int ...> struct Sequence { };

struct UnpackError
{
    explicit UnpackError(std::string reason) : reason(std::move(reason)) {}
    std::string reason;
};

} // namespace internal


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
    template<int N, typename... Ts> using NthTypeOf =
        typename std::tuple_element<N, std::tuple<Ts...>>::type;

    template <int ...S>
    void invoke(Event&& event, internal::Sequence<S...>);

    template <int N>
    static NthTypeOf<N, TArgs...> get(const Array& args);

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
EventUnpacker<TSlot, TArgs...> unpackedEvent(TSlot&& slot);



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
    template<int N, typename... Ts> using NthTypeOf =
        typename std::tuple_element<N, std::tuple<Ts...>>::type;

    template <int ...S>
    Outcome invoke(Invocation&& inv, internal::Sequence<S...>);

    template <int N>
    static NthTypeOf<N, TArgs...> get(const Array& args);

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
InvocationUnpacker<TSlot, TArgs...> unpackedRpc(TSlot&& slot);


} // namespace wamp

#include "./internal/unpacker.ipp"

#endif // CPPWAMP_UNPACKER_HPP
