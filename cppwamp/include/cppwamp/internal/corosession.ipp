/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

namespace wamp
{

//------------------------------------------------------------------------------
/** @copydetails Session::create(const Connector::Ptr&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    AsioService& userIosvc,         /**< IO service used for executing
                                         user handlers. */
    const Connector::Ptr& connector /**< Connection details for the transport
                                         to use. */
)
{
    return Ptr(new CoroSession(userIosvc, {connector}));
}

//------------------------------------------------------------------------------
/** @copydetails Session::create(const ConnectorList&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    AsioService& userIosvc,         /**< IO service used for executing
                                         user handlers. */
    const ConnectorList& connectors /**< A list of connection details for
                                         the transports to use. */
)
{
    return Ptr(new CoroSession(userIosvc, connectors));
}

//------------------------------------------------------------------------------
/** @copydetails Session::connect
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
size_t CoroSession<B>::connect(
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
                                instead of throwing an exception upon failure. */
)
{
    CPPWAMP_LOGIC_CHECK(!this->impl(), "Session is already connected");
    return run<size_t>(yield, ec, [this](CoroHandler<H, size_t>& handler)
    {
        this->connect(handler);
    });
}

//------------------------------------------------------------------------------
/** @copydetails Session::join
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
SessionInfo CoroSession<B>::join(
    Realm realm,           /**< Details on the realm to join. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
                                instead of throwing an exception upon failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::closed,
                        "Session is not closed");
    return run<SessionInfo>(yield, ec,
        [this, &realm](CoroHandler<H, SessionInfo>& handler)
        {
            this->join(std::move(realm), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Session::leave
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Reason CoroSession<B>::leave(
    Reason reason,         /**< _Reason_ URI and other details to send to
                                the router. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
                                instead of throwing an exception upon failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<Reason>(yield, ec,
        [this, &reason](CoroHandler<H, Reason>& handler)
        {
            this->leave(std::move(reason), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Session::subscribe
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Subscription CoroSession<B>::subscribe(
    Topic topic,           /**< Details on the topic to subscribe to. */
    EventSlot slot,        /**< The callable target to invoke when a matching
                                event is received. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Pointer to an optional error code to set instead
                                of throwing an exception upon failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");

    return run<Subscription>(yield, ec,
        [this, &topic, &slot](CoroHandler<H, Subscription>& handler)
        {
            this->subscribe(std::move(topic), std::move(slot), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Session::unsubscribe(const Subscription&, AsyncHandler<bool>)
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
bool CoroSession<B>::unsubscribe(
    const Subscription& sub, /**< The subscription to unsubscribe from. */
    YieldContext<H> yield,   /**< Represents the current coroutine. */
    std::error_code* ec      /**< Optional pointer to an error code to set,
                                  instead of throwing an exception upon failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<bool>(yield, ec, [this, sub](CoroHandler<H, bool>& handler)
    {
        this->unsubscribe(std::move(sub), handler);
    });
}

//------------------------------------------------------------------------------
/** @copydetails Session::publish
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
PublicationId CoroSession<B>::publish(
    Pub pub,                /**< The publication to publish */
    YieldContext<H> yield,  /**< Represents the current coroutine. */
    std::error_code* ec     /**< Optional error code to set, instead of
                                 throwing an exception. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<PublicationId>(yield, ec,
        [this, &pub](CoroHandler<H, PublicationId>& handler)
        {
            this->publish(std::move(pub), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Session::enroll
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Registration CoroSession<B>::enroll(
    Procedure procedure,   /**< The procedure URI to register. */
    CallSlot slot,         /**< Callable target to invoke when a matching RPC
                                invocation is received. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
                                instead of throwing an exception upon
                                failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");

    return run<Registration>(yield, ec,
        [this, &procedure, &slot](CoroHandler<H, Registration>& handler)
        {
            this->enroll(std::move(procedure), std::move(slot), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Session::unregister(const Registration&, AsyncHandler<bool>)
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
bool CoroSession<B>::unregister(
    const Registration& reg, /**< The RPC registration to unregister. */
    YieldContext<H> yield,   /**< Represents the current coroutine. */
    std::error_code* ec      /**< Optional pointer to an error code to set,
                                  instead of throwing an exception upon failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<bool>(yield, ec, [this, reg](CoroHandler<H, bool>& handler)
    {
        this->unregister(std::move(reg), handler);
    });
}

//------------------------------------------------------------------------------
/** @copydetails Session::call
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Result CoroSession<B>::call(
    Rpc rpc,                /**< Details on the RPC to call. */
    YieldContext<H> yield,  /**< Represents the current coroutine. */
    std::error_code* ec     /**< Optional error code to set, instead of
                                 throwing an exception. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    return run<Result>(yield, ec,
        [this, &rpc](CoroHandler<H, Result>& handler)
        {
            this->call(std::move(rpc), handler);
        });
}

//------------------------------------------------------------------------------
/** @details
    Has the same effect as
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    this->userIosvc().post(yield);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
void CoroSession<B>::suspend(YieldContext<H> yield)
{
    this->userIosvc().post(yield);
}

//------------------------------------------------------------------------------
template <typename B>
template <typename TResult, typename TYieldContext, typename TDelegate>
TResult CoroSession<B>::run(TYieldContext&& yield, std::error_code* ec,
                            TDelegate&& delegate)
{
    using boost::asio::handler_type;
    using Handler = typename handler_type<TYieldContext,
                                          void(AsyncResult<TResult>)>::type;
    Handler handler(yield);
    boost::asio::async_result<Handler> result(handler);
    delegate(handler);

    if (!ec)
        return result.get().get();
    else
    {
        auto finalResult = result.get();
        *ec = finalResult.errorCode();
        return finalResult ? finalResult.get() : TResult();
    }
}


} // namespace wamp
