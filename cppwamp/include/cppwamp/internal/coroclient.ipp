/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "registrationimpl.hpp"
#include "subscriptionimpl.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** @copydetails Client::create(const Connector::Ptr&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroClient<B>::Ptr CoroClient<B>::create(
    const Connector::Ptr& connector /**< Connection details for the transport
                                         to use. */
)
{
    return Ptr(new CoroClient(connector));
}

//------------------------------------------------------------------------------
/** @copydetails Client::create(const ConnectorList&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroClient<B>::Ptr CoroClient<B>::create(
    const ConnectorList& connectors /**< A list of connection details for
                                         the transports to use. */
)
{
    return Ptr(new CoroClient(connectors));
}

//------------------------------------------------------------------------------
/** @copydetails Client::connect
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
size_t CoroClient<B>::connect(
    YieldContext<H> yield /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(!this->impl(), "Session is already connected");
    return run<size_t>(yield, [this](CoroHandler<H, size_t>& handler)
    {
        this->connect(handler);
    });
}

//------------------------------------------------------------------------------
/** @copydetails Client::join
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
SessionId CoroClient<B>::join(
    std::string realm,     /**< The realm to join to. */
    YieldContext<H> yield  /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::closed,
                        "Session is not closed");
    return run<SessionId>(yield,
        [this, &realm](CoroHandler<H, SessionId>& handler)
        {
            this->join(std::move(realm), handler);
        });
}

//------------------------------------------------------------------------------
/** @details
    Same as leave(std::string, YieldContext<H>), except that no
    _Reason_ URI is specified. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
std::string CoroClient<B>::leave(
    YieldContext<H> yield  /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<std::string>(yield,
        [this](CoroHandler<H, std::string>& handler) {this->leave(handler);});
}

//------------------------------------------------------------------------------
/** @copydetails Client::leave(std::string, AsyncHandler<std::string>)
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
std::string CoroClient<B>::leave(
    std::string reason,     /**< _Reason_ URI to send to the router. */
    YieldContext<H> yield   /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<std::string>(yield,
        [this, &reason](CoroHandler<H, std::string>& handler)
        {
            this->leave(std::move(reason), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Client::subscribe
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename... TParams, typename TEventSlot, typename H>
Subscription CoroClient<B>::subscribe(
    std::string topic,    /**< The topic URI to subscribe to. */
    TEventSlot&& slot,    /**< Universal reference to a callable target to
                               invoke when a matching event is received. */
    YieldContext<H> yield /**< Represents the current coroutine. */
)
{
    static_assert(sizeof...(TParams) > 0, "At least one template parameter is "
                                     "required for the slot's signature");
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");

    using Sub = internal::SubscriptionImpl<TParams...>;
    using Slot = typename Sub::Slot;
    auto sub = Sub::create(this->impl(), std::move(topic), Slot(slot));

    return run<Subscription>(yield,
        [this, &topic, &sub](CoroHandler<H, Subscription>& handler)
        {
            this->doSubscribe(std::move(sub), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Client::unsubscribe(Subscription, AsyncHandler<bool>)
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
void CoroClient<B>::unsubscribe(
    Subscription sub,     /**< The subscription to unsubscribe from. */
    YieldContext<H> yield /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    run<bool>(yield, [this, sub](CoroHandler<H, bool>& handler)
    {
        this->unsubscribe(std::move(sub), handler);
    });
}

//------------------------------------------------------------------------------
/** @copydetails Client::publish(std::string, AsyncHandler<PublicationId>)
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
PublicationId CoroClient<B>::publish(
    std::string topic,    /**< The topic URI under which to publish. */
    YieldContext<H> yield /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<PublicationId>(yield,
        [this, &topic](CoroHandler<H, PublicationId>& handler)
        {
            this->publish(std::move(topic), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Client::publish(std::string, Args, AsyncHandler<PublicationId>)
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
PublicationId CoroClient<B>::publish(
    std::string topic,    /**< The topic URI under which to publish. */
    Args args,            /**< Positional and/or keyword values to supply for
                               the event payload. */
    YieldContext<H> yield /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<PublicationId>(yield,
        [this, &topic, &args](CoroHandler<H, PublicationId>& handler)
        {
            this->publish(std::move(topic), std::move(args), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Client::enroll
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename... TParams, typename TCallSlot, typename H>
Registration CoroClient<B>::enroll(
    std::string procedure, /**< The procedure URI to register. */
    TCallSlot&& slot,      /**< Universal reference to a callable target to
                                invoke when a matching RPC invocation is
                                received. */
    YieldContext<H> yield  /**< Represents the current coroutine. */
)
{
    static_assert(sizeof...(TParams) > 0, "At least one template parameter is "
                                          "required for the slot's signature");
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");

    using Reg = internal::RegistrationImpl<TParams...>;
    using Slot = typename Reg::Slot;
    auto reg = Reg::create(this->impl(), std::move(procedure), Slot(slot));

    return run<Registration>(yield,
        [this, &procedure, &reg](CoroHandler<H, Registration>& handler)
        {
            this->doEnroll(std::move(reg), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Client::unregister(Registration, AsyncHandler<bool>)
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
void CoroClient<B>::unregister(
    Registration reg,     /**< The RPC registration to unregister. */
    YieldContext<H> yield /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    run<bool>(yield, [this, reg](CoroHandler<H, bool>& handler)
    {
        this->unregister(std::move(reg), handler);
    });
}

//------------------------------------------------------------------------------
/** @copydetails Client::call(std::string, AsyncHandler<Args>)
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Args CoroClient<B>::call(
    std::string procedure, /**< The procedure URI to call. */
    YieldContext<H> yield  /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<Args>(yield,
        [this, &procedure](CoroHandler<H, Args>& handler)
        {
            this->call(std::move(procedure), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Client::call(std::string, Args, AsyncHandler<Args>)
    @throws error::Wamp with an error code if a runtime error occured */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Args CoroClient<B>::call(
    std::string procedure, /**< The procedure URI to call. */
    Args args,             /**< Positional and/or keyword arguments to be
                                passed to the RPC. */
    YieldContext<H> yield  /**< Represents the current coroutine. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<Args>(yield,
        [this, &procedure, &args](CoroHandler<H, Args>& handler)
        {
            this->call(std::move(procedure), std::move(args), handler);
        });
}

//------------------------------------------------------------------------------
/** @details
    Has the same effect as
    ~~~~~~~~~~~~~~~~~~
    iosvc.post(yield);
    ~~~~~~~~~~~~~~~~~~
    where `iosvc` is the asynchronous I/O service used by the client's
    underlying transport.

    @pre The client must have already established a transport connection. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
void CoroClient<B>::suspend(YieldContext<H> yield)
{
    CPPWAMP_LOGIC_CHECK(!!this->impl(), "Session is not connected");
    using boost::asio::handler_type;
    using Handler = typename handler_type<YieldContext<H>, void()>::type;
    Handler handler(yield);
    boost::asio::async_result<Handler> result(handler);
    this->postpone(handler);
    return result.get();
}

//------------------------------------------------------------------------------
template <typename B>
template <typename TResult, typename TYieldContext, typename TDelegate>
TResult CoroClient<B>::run(TYieldContext&& yield, TDelegate&& delegate)
{
    using boost::asio::handler_type;
    using Handler = typename handler_type<TYieldContext,
                                          void(AsyncResult<TResult>)>::type;
    Handler handler(yield);
    boost::asio::async_result<Handler> result(handler);
    delegate(handler);
    return result.get().get();
}

} // namespace wamp
