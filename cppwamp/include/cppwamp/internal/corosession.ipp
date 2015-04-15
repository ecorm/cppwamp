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
/** @copydetails Session::create(const Connector::Ptr&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    const Connector::Ptr& connector /**< Connection details for the transport
                                         to use. */
)
{
    return Ptr(new CoroSession(connector));
}

//------------------------------------------------------------------------------
/** @copydetails Session::create(const ConnectorList&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    const ConnectorList& connectors /**< A list of connection details for
                                         the transports to use. */
)
{
    return Ptr(new CoroSession(connectors));
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
Subscription::Ptr CoroSession<B>::subscribe(
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

    auto sub = internal::DynamicSubscription::create(this->impl(),
            std::move(topic), std::move(slot));

    return run<Subscription::Ptr>(yield, ec,
        [this, &sub](CoroHandler<H, Subscription::Ptr>& handler)
        {
            this->doSubscribe(std::move(sub), handler);
        });
}

//------------------------------------------------------------------------------
/** @details
    This overload is used to register an _event slot_ that takes additional,
    statically-typed payload arguments.

    A _slot_ is a function that is called in response to a _signal_ (the signal
    being the event topic in this case). The term _slot_, borrowed from
    [Qt's signals and slots][qt_sig], is used to distinguish the event handler
    from asynchronous operation handlers.

    For this `subscribe` overload, an event slot must be a _callable target_
    with the following signature:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void function(Event, THead, TTail...)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where
    - _callable target_ is a free function, bound member function,
      function object, lambda function, etc.,
    - `Event` is an object containing information related to the publication,
    - `THead` is the first template parameter passed to Session::subscribe, and,
    - `TTail` is zero or more additional template parameters that were passed
      to Session::subscribe.

    Examples of compliant slots are:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void onSensorSampled(Event event, float value) { ... }
    //                           THead^^^^^

    void onPurchase(Event event, std::string item, int cost, int qty) { ... }
    //                                  ^          ^           ^
    //                                THead        |---TTail---|
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    The above slots are registered as follows:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    session->subscribe<float>("sensorSampled", &onSensorSampled, handler);

    session->subscribe<std::string, int, int>("itemPurchased", &onPurchase,
                                              handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    where `handler` is the asynchronous handler (or coroutine yield context)
    for the **subscribe operation itself**.

    Whenever a Session dispatches an event to the above slots, it automatically
    unmarshalls the event payload positional arguments, and passes them to the
    slots' argument list. If Session cannot convert the event payload arguments
    to their target types, it issues a warning that can be captured via
    Session::setWarningHandler.

    [qt_sig]: http://doc.qt.io/qt-5/signalsandslots.html

    @tparam THead The first parameter type that the slot function expects
            after the Event parameter.
    @tparam TTail List of additional parameter types, following THead, that the
            slot function expects after the Event parameter.
    @tparam TEventSlot Callable target type of the slot. Must be move-assignable
            to `std::function<Event, THead, TTail...>`.
    @return A `std::shared_ptr` to a Subscription object, therafter used to
            manage the subscription's lifetime.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::subscribeError if the router replied with an `ERROR`
          response.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established`
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename THead, typename... TTail, typename TEventSlot, typename H>
Subscription::Ptr CoroSession<B>::subscribe(
    Topic topic,           /**< Details on the topic to subscribe to. */
    TEventSlot&& slot,     /**< Universal reference to a callable target to
                                invoke when a matching event is received. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Pointer to an optional error code to set instead
                                of throwing an exception upon failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");

    using Sub = internal::StaticSubscription<THead, TTail...>;
    auto sub = Sub::create(this->impl(), std::move(topic),
                           std::forward<TEventSlot>(slot));

    return run<Subscription::Ptr>(yield, ec,
        [this, &sub](CoroHandler<H, Subscription::Ptr>& handler)
        {
            this->doSubscribe(std::move(sub), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Session::unsubscribe(Subscription::Ptr, AsyncHandler<bool>)
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
bool CoroSession<B>::unsubscribe(
    Subscription::Ptr sub, /**< The subscription to unsubscribe from. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
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
Registration::Ptr CoroSession<B>::enroll(
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

    auto reg = internal::DynamicRegistration::create(this->impl(),
            std::move(procedure), std::move(slot));

    return run<Registration::Ptr>(yield, ec,
        [this, &procedure, &reg](CoroHandler<H, Registration::Ptr>& handler)
        {
            this->doEnroll(std::move(reg), handler);
        });
}

//------------------------------------------------------------------------------
/** @details
    This overload is used to register a _call slot_ that takes additional,
    statically-typed payload arguments.

    A _slot_ is a function that is called in response to a _signal_ (the signal
    being the call invocation in this case). The term _slot_, borrowed from
    [Qt's signals and slots][qt_sig], is used to distinguish the call handler
    from asynchronous operation handlers.

    For this `enroll` overload, an event slot must be a _callable target_ with
    the following signature:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void function(Invocation, THead, TTail...)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where
    - _callable target_ is a free function, bound member function,
      function object, lambda function, etc,
    - `Invocation` is an object containing information related to the
      invocation,
    - `THead` is the first template parameter passed to Session::enroll, and,
    - `TTail` is zero or more additional template parameters that were passed
      to Session::enroll.

    Examples of compliant slots are:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void setSpeed(Invocation inv, float speed) { ... }
    //                       THead^^^^^

    void purchase(Invocation inv, std::string item, int cost, int qty) { ... }
    //                                   ^          ^           ^
    //                                 THead        |---TTail---|
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    The above slots are registered as follows:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    session->enroll<float>("setSpeed", &setSpeed, handler);

    session->enroll<std::string, int, int>("purchase", &purchase, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    where `handler` is the asynchronous handler (or coroutine yield context)
    for the **enroll operation itself**.

    Whenever a Session dispatches a remote procedure call to the above slots,
    it automatically unmarshalls the invocation payload positional arguments,
    and passes them to the slots' argument list. If Session cannot convert the
    invocation payload arguments to their target types, it automatically sends
    an `ERROR` reply back to the router.

    [qt_sig]: http://doc.qt.io/qt-5/signalsandslots.html

    @tparam THead The first parameter type that the slot function expects
            after the Event parameter.
    @tparam TTail List of additional parameter types, following THead, that the
            slot function expects after the Event parameter.
    @tparam TCallSlot Callable target type of the slot. Must be move-assignable
            to `std::function<Invocation, THead, TTail...>`.
    @return A `shared_ptr` to a Registration handle, therafter used to manage
            the registration's lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::procedureAlreadyExists if the router reports that the
          procedure has already been registered for this realm.
        - SessionErrc::registerError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established`
    @throws error::Failure with an error code if a runtime error occured and
                the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename THead, typename... TTail, typename TCallSlot, typename H>
Registration::Ptr CoroSession<B>::enroll(
    Procedure procedure,   /**< The procedure URI to register. */
    TCallSlot&& slot,      /**< Callable target to invoke when a matching
                                RPC invocation is received. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
                                instead of throwing an exception upon
                                failure. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");

    using Reg = internal::StaticRegistration<THead, TTail...>;
    auto reg = Reg::create(this->impl(), std::move(procedure),
                           std::forward<TCallSlot>(slot));

    return run<Registration::Ptr>(yield, ec,
        [this, &procedure, &reg](CoroHandler<H, Registration::Ptr>& handler)
        {
            this->doEnroll(std::move(reg), handler);
        });
}

//------------------------------------------------------------------------------
/** @copydetails Session::unregister(Registration::Ptr, AsyncHandler<bool>)
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
bool CoroSession<B>::unregister(
    Registration::Ptr reg, /**< The RPC registration to unregister. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
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
    ~~~~~~~~~~~~~~~~~~
    iosvc.post(yield);
    ~~~~~~~~~~~~~~~~~~
    where `iosvc` is the asynchronous I/O service used by the client's
    underlying transport.

    @pre The client must have already established a transport connection. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
void CoroSession<B>::suspend(YieldContext<H> yield)
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
