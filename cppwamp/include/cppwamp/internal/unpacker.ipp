/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <sstream>
#include "varianttraits.hpp"

namespace wamp
{

#ifndef CPPWAMP_FOR_DOXYGEN // GenSequence confuses the Doxygen parse
namespace internal
{

template<int N, int ...S>
struct GenSequence : GenSequence<N-1, N-1, S...> { };

template<int ...S>
struct GenSequence<0, S...>
{
    using type = Sequence<S...>;
};

} // namespace internal
#endif

//------------------------------------------------------------------------------
template <typename S, typename... A>
EventUnpacker<S,A...>::EventUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

template <typename S, typename... A>
void EventUnpacker<S,A...>::operator()(Event&& event)
{
    if (event.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << event.args().size();
        throw internal::UnpackError(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenSequence<sizeof...(A)>::type;
    invoke(std::move(event), Seq());
}

template <typename S, typename... A>
template <int ...Seq>
void EventUnpacker<S,A...>::invoke(Event&& event, internal::Sequence<Seq...>)
{
    Array args = event.args();
    slot_(std::move(event), get<Seq>(args)...);
}

template <typename S, typename... A>
template <int N>
EventUnpacker<S,A...>::NthTypeOf<N, A...>
EventUnpacker<S,A...>::get(const Array& args)
{
    using TargetType = NthTypeOf<N, A...>;
    try
    {
        return args.at(N).to<TargetType>();
    }
    catch(const error::Conversion&)
    {
        std::ostringstream oss;
        oss << "Expected type " << internal::ArgTraits<TargetType>::typeName()
            << " for arg index " << N
            << ", but got type " << typeNameOf(args.at(N));
        throw internal::UnpackError(oss.str());
    }
}

template <typename... TArgs, typename TSlot>
EventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedEvent(TSlot&& slot)
{
    return EventUnpacker<DecayedSlot<TSlot>, TArgs...>(
                std::forward<TSlot>(slot));
}


//------------------------------------------------------------------------------
template <typename S, typename... A>
InvocationUnpacker<S,A...>::InvocationUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

template <typename S, typename... A>
Outcome InvocationUnpacker<S,A...>::operator()(Invocation&& inv)
{
    if (inv.args().size() < sizeof...(A))
    {
        std::ostringstream oss;
        oss << "Expected " << sizeof...(A)
            << " args, but only got " << inv.args().size();
        throw internal::UnpackError(oss.str());
    }

    // Use the integer parameter pack technique shown in
    // http://stackoverflow.com/a/7858971/245265
    using Seq = typename internal::GenSequence<sizeof...(A)>::type;
    return invoke(std::move(inv), Seq());
}

template <typename S, typename... A>
template <int ...Seq>
Outcome InvocationUnpacker<S,A...>::invoke(Invocation&& inv,
                                           internal::Sequence<Seq...>)
{
    Array args = inv.args();
    return slot_(std::move(inv), get<Seq>(args)...);
}

template <typename S, typename... A>
template <int N>
InvocationUnpacker<S,A...>::NthTypeOf<N, A...>
InvocationUnpacker<S,A...>::get(const Array& args)
{
    using TargetType = NthTypeOf<N, A...>;
    try
    {
        return args.at(N).to<TargetType>();
    }
    catch(const error::Conversion&)
    {
        std::ostringstream oss;
        oss << "Expected type " << internal::ArgTraits<TargetType>::typeName()
            << " for arg index " << N
            << ", but got type " << typeNameOf(args.at(N));
        throw internal::UnpackError(oss.str());
    }
}

template <typename... TArgs, typename TSlot>
InvocationUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedRpc(TSlot&& slot)
{
    return InvocationUnpacker<DecayedSlot<TSlot>, TArgs...>(
                std::forward<TSlot>(slot) );
}

} // namespace wamp
