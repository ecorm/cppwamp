/*------------------------------------------------------------------------------
                     Copyright Emile Cormier 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>

namespace wamp
{

namespace internal
{

// Based on http://stackoverflow.com/a/10281114/245265
template <typename T>
struct MoveOnCopyWrapper
{
    mutable T value;

    MoveOnCopyWrapper(T&& t): value(std::move(t)) {}

    MoveOnCopyWrapper(MoveOnCopyWrapper const& other)
        : value(std::move(other.value))
    {}

    MoveOnCopyWrapper(MoveOnCopyWrapper&& other)
        : value(std::move(other.value))
    {}

    MoveOnCopyWrapper& operator=(MoveOnCopyWrapper const& other)
    {
        value = std::move(other.value);
        return *this;
    }

    MoveOnCopyWrapper& operator=(MoveOnCopyWrapper&& other)
    {
        value = std::move(other.value);
        return *this;
    }
};

template <typename T>
MoveOnCopyWrapper<T> moveOnCopy(T&& t)
{
    return MoveOnCopyWrapper<T>(std::move(t));
}

} // namespace internal

//------------------------------------------------------------------------------
/// @copydetails Session::create(const Connector::Ptr&)
template <typename B>
typename FutuSession<B>::Ptr FutuSession<B>::create(
        const Connector::Ptr& connector)
{
    return Ptr(new FutuSession(connector));
}

//------------------------------------------------------------------------------
/// @copydetails Session::create(const ConnectorList&)
template <typename B>
typename FutuSession<B>::Ptr FutuSession<B>::create(
        const ConnectorList& connectors)
{
    return Ptr(new FutuSession(connectors));
}

//------------------------------------------------------------------------------
/// @copydetails Session::connect
template <typename B>
FutuSession<B>::Future<size_t> FutuSession<B>::connect()
{
    return run<size_t>([this](AsyncHandler<size_t>&& handler)
    {
        this->connect(std::move(handler));
    });
}

//------------------------------------------------------------------------------
/// @copydetails Session::join
template <typename B>
FutuSession<B>::Future<SessionInfo> FutuSession<B>::join(Realm realm)
{
    return run<SessionInfo>([this, &realm](AsyncHandler<SessionInfo>&& handler)
    {
        this->join(std::move(realm), std::move(handler));
    });
}

//------------------------------------------------------------------------------
/// @copydetails Session::leave
template <typename B>
FutuSession<B>::Future<Reason> FutuSession<B>::leave(Reason reason)
{
    return run<Reason>([this, &reason](AsyncHandler<Reason>&& handler)
    {
        this->leave(std::move(reason), std::move(handler));
    });
}

//------------------------------------------------------------------------------
/// @copydetails Session::subscribe
template <typename B>
FutuSession<B>::Future<Subscription> FutuSession<B>::subscribe(Topic topic,
                                                               EventSlot slot)
{
    using std::move;
    return run<Subscription>(
        [this, &topic, &slot](AsyncHandler<Subscription>&& handler)
        {
            this->subscribe(move(topic), move(slot), move(handler));
        });
}

//------------------------------------------------------------------------------
/// @copydetails Session::unsubscribe(const Subscription&, AsyncHandler<bool>)
template <typename B>
FutuSession<B>::Future<bool>
FutuSession<B>::unsubscribe(const Subscription& sub, WithFuture)
{
    return run<bool>([this, &sub](AsyncHandler<bool>&& handler)
    {
        this->unsubscribe(sub, std::move(handler));
    });
}

//------------------------------------------------------------------------------
/// @copydetails Session::publish
template <typename B>
FutuSession<B>::Future<PublicationId> FutuSession<B>::publish(Pub pub,
                                                              WithFuture)
{
    return run<PublicationId>(
        [this, &pub](AsyncHandler<PublicationId>&& handler)
        {
            this->publish(std::move(pub), std::move(handler));
        });
}

//------------------------------------------------------------------------------
/// @copydetails Session::enroll
template <typename B>
FutuSession<B>::Future<Registration> FutuSession<B>::enroll(Procedure procedure,
                                                            CallSlot slot)
{
    using std::move;
    return run<Registration>(
        [this, &procedure, &slot](AsyncHandler<Registration>&& handler)
        {
            this->enroll(move(procedure), move(slot), move(handler));
        });
}

//------------------------------------------------------------------------------
/// @copydetails Session::unregister(const Registration&, AsyncHandler<bool>)
template <typename B>
FutuSession<B>::Future<bool> FutuSession<B>::unregister(const Registration& reg,
                                                        WithFuture)
{
    return run<bool>([this, &reg](AsyncHandler<bool>&& handler)
    {
        this->unregister(reg, std::move(handler));
    });
}

//------------------------------------------------------------------------------
/// @copydetails Session::call
template <typename B>
FutuSession<B>::Future<Result> FutuSession<B>::call(Rpc rpc)
{
    return run<Result>([this, &rpc](AsyncHandler<Result>&& handler)
    {
        this->call(std::move(rpc), std::move(handler));
    });
}

//------------------------------------------------------------------------------
template <typename B>
template <typename TResult, typename TDelegate>
FutuSession<B>::Future<TResult> FutuSession<B>::run(TDelegate delegate)
{
    auto prom = internal::moveOnCopy(boost::promise<TResult>());
    auto fut = prom.value.get_future();
    AsyncHandler<TResult> handler = [prom](AsyncResult<TResult> result)
    {
        if (result)
            prom.value.set_value(result.get());
        else
        {
            error::Failure e(result.errorCode(), result.errorInfo());
            prom.value.set_exception(std::move(e));
        }
    };
    delegate(std::move(handler));
    return std::move(fut);
}
} // namespace wamp
