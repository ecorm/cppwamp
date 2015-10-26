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

namespace internal
{

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
        catch(const error::Conversion&)
        {
            std::ostringstream oss;
            oss << "Expected type " << ArgTraits<TargetType>::typeName()
                << " for arg index " << N
                << ", but got type " << typeNameOf(args.at(N));
            throw UnpackError(oss.str());
        }
    }
};

} // namespace internal


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
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    invoke(std::move(event), Seq());
}

template <typename S, typename... A>
template <int ...Seq>
void EventUnpacker<S,A...>::invoke(Event&& event,
                                   internal::IntegerSequence<Seq...>)
{
    Array args = event.args();
    using Getter = internal::UnpackedArgGetter<A...>;
    slot_(std::move(event), Getter::template get<Seq>(args)...);
}

template <typename... TArgs, typename TSlot>
EventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedEvent(TSlot&& slot)
{
    return EventUnpacker<DecayedSlot<TSlot>, TArgs...>(
                std::forward<TSlot>(slot));
}


//------------------------------------------------------------------------------
template <typename S, typename... A>
BasicEventUnpacker<S,A...>::BasicEventUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

template <typename S, typename... A>
void BasicEventUnpacker<S,A...>::operator()(Event&& event)
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
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    invoke(std::move(event), Seq());
}

template <typename S, typename... A>
template <int ...Seq>
void BasicEventUnpacker<S,A...>::invoke(Event&& event,
                                       internal::IntegerSequence<Seq...>)
{
    Array args = event.args();
    using Getter = internal::UnpackedArgGetter<A...>;
    slot_(Getter::template get<Seq>(args)...);
}

template <typename... TArgs, typename TSlot>
BasicEventUnpacker<DecayedSlot<TSlot>, TArgs...> basicEvent(TSlot&& slot)
{
    return BasicEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
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
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    return invoke(std::move(inv), Seq());
}

template <typename S, typename... A>
template <int ...Seq>
Outcome InvocationUnpacker<S,A...>::invoke(Invocation&& inv,
                                           internal::IntegerSequence<Seq...>)
{
    Array args = inv.args();
    using Getter = internal::UnpackedArgGetter<A...>;
    return slot_(std::move(inv), Getter::template get<Seq>(args)...);
}

template <typename... TArgs, typename TSlot>
InvocationUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedRpc(TSlot&& slot)
{
    return InvocationUnpacker<DecayedSlot<TSlot>, TArgs...>(
                std::forward<TSlot>(slot) );
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
BasicInvocationUnpacker<S,R,A...>::BasicInvocationUnpacker(Slot slot)
    : slot_(std::move(slot))
{}

template <typename S, typename R, typename... A>
Outcome BasicInvocationUnpacker<S,R,A...>::operator()(Invocation&& inv)
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
    using Seq = typename internal::GenIntegerSequence<sizeof...(A)>::type;
    using IsVoidResult = BoolConstant<std::is_same<ResultType, void>::value>;
    return invoke(IsVoidResult{}, std::move(inv), Seq());
}

template <typename S, typename R, typename... A>
template <int ...Seq>
Outcome BasicInvocationUnpacker<S,R,A...>::invoke(TrueType, Invocation&& inv,
        internal::IntegerSequence<Seq...>)
{
    Array args = inv.args();
    using Getter = internal::UnpackedArgGetter<A...>;
    slot_(Getter::template get<Seq>(args)...);
    return {};
}

template <typename S, typename R, typename... A>
template <int ...Seq>
Outcome BasicInvocationUnpacker<S,R,A...>::invoke(FalseType, Invocation&& inv,
            internal::IntegerSequence<Seq...>)
{
    Array args = inv.args();
    using Getter = internal::UnpackedArgGetter<A...>;
    ResultType result = slot_(Getter::template get<Seq>(args)...);
    return wamp::Result().withArgs(std::move(result));
}

template <typename TResult, typename... TArgs, typename TSlot>
BasicInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicRpc(TSlot&& slot)
{
    return BasicInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
                std::forward<TSlot>(slot) );
}

} // namespace wamp
