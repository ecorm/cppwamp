/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <sstream>
#include "../peerdata.hpp"

namespace wamp
{

namespace internal
{

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


//------------------------------------------------------------------------------
template <typename S, typename... A>
CoroEventUnpacker<S,A...>::CoroEventUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

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

template <typename S, typename... A>
template <int ...Seq>
void CoroEventUnpacker<S,A...>::invoke(Event&& event,
                                   internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(event.iosvc(), [slot, event](Yield yield)
    {
        Array args = event.args();
        using Getter = internal::UnpackedCoroArgGetter<A...>;
        (*slot)(std::move(event), Getter::template get<Seq>(args)..., yield);
    });
}

template <typename... TArgs, typename TSlot>
CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...> unpackedCoroEvent(TSlot&& slot)
{
    return CoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
                std::forward<TSlot>(slot));
}


//------------------------------------------------------------------------------
template <typename S, typename... A>
BasicCoroEventUnpacker<S,A...>::BasicCoroEventUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

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

template <typename S, typename... A>
template <int ...Seq>
void BasicCoroEventUnpacker<S,A...>::invoke(Event&& event,
                                       internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    Array args = std::move(event).args();

    boost::asio::spawn(event.iosvc(), [slot, args](Yield yield)
    {
        using Getter = internal::UnpackedCoroArgGetter<A...>;
        (*slot)(Getter::template get<Seq>(args)..., yield);
    });
}

template <typename... TArgs, typename TSlot>
BasicCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>
basicCoroEvent(TSlot&& slot)
{
    return BasicCoroEventUnpacker<DecayedSlot<TSlot>, TArgs...>(
                std::forward<TSlot>(slot));
}


//------------------------------------------------------------------------------
template <typename S, typename... A>
CoroInvocationUnpacker<S,A...>::CoroInvocationUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

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

template <typename S, typename... A>
template <int ...Seq>
void CoroInvocationUnpacker<S,A...>::invoke(Invocation&& inv,
                                           internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(inv.iosvc(), [slot, inv](Yield yield)
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
    });
}

template <typename... TArgs, typename TSlot>
CoroInvocationUnpacker<DecayedSlot<TSlot>, TArgs...>
unpackedCoroRpc(TSlot&& slot)
{
    return CoroInvocationUnpacker<DecayedSlot<TSlot>, TArgs...>(
                std::forward<TSlot>(slot) );
}

//------------------------------------------------------------------------------
template <typename S, typename R, typename... A>
BasicCoroInvocationUnpacker<S,R,A...>::BasicCoroInvocationUnpacker(Slot slot)
    : slot_(std::make_shared<Slot>(std::move(slot)))
{}

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

template <typename S, typename R, typename... A>
template <int ...Seq>
void BasicCoroInvocationUnpacker<S,R,A...>::invoke(TrueType, Invocation&& inv,
        internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(inv.iosvc(), [slot, inv](Yield yield)
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
    });
}

template <typename S, typename R, typename... A>
template <int ...Seq>
void BasicCoroInvocationUnpacker<S,R,A...>::invoke(FalseType, Invocation&& inv,
            internal::IntegerSequence<Seq...>)
{
    auto slot = slot_;
    boost::asio::spawn(inv.iosvc(), [slot, inv](Yield yield)
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
    });
}

template <typename TResult, typename... TArgs, typename TSlot>
BasicCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>
basicCoroRpc(TSlot&& slot)
{
    return BasicCoroInvocationUnpacker<DecayedSlot<TSlot>, TResult, TArgs...>(
                std::forward<TSlot>(slot) );
}

} // namespace wamp
