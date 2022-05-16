/*------------------------------------------------------------------------------
            Copyright Butterfly Energy Systems 2014-2015, 2018, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CORO_COROSESSION_HPP
#define CPPWAMP_CORO_COROSESSION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the coroutine-based API used by a _client_ peer in WAMP
           applications. */
//------------------------------------------------------------------------------

#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include "../session.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Coroutine API used by a _client_ peer in WAMP applications.

This mixin class adds a _coroutine_ API on top of the asynchronous one
provided by Session. Coroutines enable client programs to implement
asynchronous logic in a synchronous manner. This class is based on the
[stackful coroutines][asiocoro] implementation used by Boost.Asio.

The asynchronous operations in Session are mapped to coroutine operations
as follows:
- A [boost::asio::yield_context][yieldcontext] is passed in place of the
  asynchronous completion handler.
- The result is returned directly by the function, instead of via an
  AsyncResult object passed to the completion handler.
- Runtime errors are thrown as error::Failure exceptions. When caught, the
  error code can be retrieved via `error::Failure::code`.
- An optional pointer to a `std::error_code` can be passed to coroutine
  operations. If a runtime error occurs, it will set the pointed-to
  error code instead of throwing an error::Failure exception.

For example, the asynchronous operation,
```
void join(std::string realm, AsyncHandler<SessionInfo> handler);
```
becomes the following equivalent coroutine operation:
```
SessionInfo join(std::string realm, YieldContext<H> yield,
                 std::error_code* ec = nullptr);
```

A yield context is obtained via [boost::asio::spawn][spawn]. For example:
```
boost::asio::io_context ioctx;
auto session = CoroSession<>::create(connectorList);
boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
{
    session->connect(yield);
    SessionInfo info = session->join("somerealm", yield);
    // etc...
});
ioctx.run();
```
[asiocoro]: http://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/spawn.html
[yieldcontext]: http://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/yield_context.html
[spawn]: http://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/spawn.html

@par Aborting Coroutine Operations
All pending coroutine operations can be _aborted_ by dropping the client
connection via Session::disconnect. Pending post-join operations can be also
be aborted via CoroSession::leave. Operations aborted in this manner will
throw an error::Failure exception. There is currently no way to abort a
single operation without dropping the connection or leaving the realm.

@par Mixins
This mixin class can be combined with other session mixin classes,
by chaining the `TBase` template parameter. For example:
```
// Mixin both the CoroSession and FutuSession APIs:
using SessionApi = FutuSession<CoroSession<>>;
auto session = SessionApi::create(connectorList);
```

@note The `FutuSession` API is not yet implemented and is shown here
      for demonstration purposes.

@tparam TBase The base class that this mixin extends.
@extends Session
@see Session, Registration, Subscription. */
//------------------------------------------------------------------------------
template <typename TBase = Session>
class CoroSession : public TBase
{
public:
    /** Shared pointer to a CoroSession. */
    using Ptr = std::shared_ptr<CoroSession>;

    /** The base class type that this mixin extends. */
    using Base = TBase;

    /** Enumerates the possible states that a CoroSession can be in. */
    using State = SessionState;

    /** Function type for handling pub/sub events. */
    using EventSlot = std::function<void (Event)>;

    /** Function type for handling remote procedure calls. */
    using CallSlot = std::function<Outcome (Invocation)>;

    /** Function type for handling RPC interruptions. */
    using InterruptSlot = std::function<Outcome (Interruption)>;

    /** Yield context type used by the boost::asio::spawn handler. */
    template <typename TSpawnHandler>
    using YieldContext = boost::asio::basic_yield_context<TSpawnHandler>;

    /** Creates a new CoroSession instance. */
    static Ptr create(AnyExecutor exec, const Connector::Ptr& connector);

    /** Creates a new CoroSession instance. */
    static Ptr create(AnyExecutor exec, const ConnectorList& connectors);

    /** Creates a new CoroSession instance.
        @copydetails Session::create(AnyExecutor, const Connector::Ptr&)
        @details Only participates in overload resolution when
                 `isExecutionContext<TExecutionContext>() == true`
        @tparam TExecutionContext Must meet the requirements of
                                  Boost.Asio's ExecutionContext */
    template <typename TExecutionContext>
    static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<TExecutionContext>())
    create(
        TExecutionContext& context, /**< Provides executor with which to
                                         post all user-provided handlers. */
        const Connector::Ptr& connector /**< Connection details for the
                                             transport to use. */
        )
    {
        return create(context.get_executor(), connector);
    }

    /** Creates a new CoroSession instance.
        @copydetails Session::create(AnyExecutor, const Connector::Ptr&)
        @details Only participates in overload resolution when
                 `isExecutionContext<TExecutionContext>() == true`
        @tparam TExecutionContext Must meet the requirements of
                                  Boost.Asio's ExecutionContext */
    template <typename TExecutionContext>
    static CPPWAMP_ENABLED_TYPE(Ptr, isExecutionContext<TExecutionContext>())
    create(
        TExecutionContext& context, /**< Provides executor with which to
                                         post all user-provided handlers. */
        const ConnectorList& connectors) /**< Connection details for the
                                              transport to use. */
    {
        return create(context.get_executor(), connectors);
    }

    using Base::connect;
    using Base::join;
    using Base::leave;
    using Base::disconnect;
    using Base::subscribe;
    using Base::unsubscribe;
    using Base::publish;
    using Base::enroll;
    using Base::unregister;
    using Base::call;
    using Base::cancel;

    /// @name Session Management
    /// @{
    /** Attempts to connect to a router. */
    template <typename H>
    size_t connect(YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Attempts to join the given WAMP realm. */
    template <typename H>
    SessionInfo join(Realm realm, YieldContext<H> yield,
                     std::error_code* ec = nullptr);

    /** Leaves the WAMP session. */
    template <typename H>
    Reason leave(YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Leaves the WAMP session with the given reason. */
    template <typename H>
    Reason leave(Reason reason, YieldContext<H> yield,
                 std::error_code* ec = nullptr);
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename H>
    Subscription subscribe(Topic topic, EventSlot slot,
            YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement if necessary. */
    template <typename H>
    bool unsubscribe(const Subscription& sub, YieldContext<H> yield,
                     std::error_code* ec = nullptr);

    /** Publishes an event and waits for an acknowledgement from the router. */
    template <typename H>
    PublicationId publish(Pub pub, YieldContext<H> yield,
                          std::error_code* ec = nullptr);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    template <typename H>
    Registration enroll(Procedure procedure, CallSlot slot,
            YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    template <typename H>
    Registration enroll(Procedure procedure, CallSlot slot,
                        InterruptSlot interruptSlot, YieldContext<H> yield,
                        std::error_code* ec = nullptr);

    /** Unregisters a remote procedure call. */
    template <typename H>
    bool unregister(const Registration& reg, YieldContext<H> yield,
                    std::error_code* ec = nullptr);

    /** Calls a remote procedure */
    template <typename H>
    Result call(Rpc rpc, YieldContext<H> yield, std::error_code* ec = nullptr);
    /// @}

    /// @name Coroutine
    /// @{
    /** Cooperatively suspend this coroutine to allow others to run. */
    template <typename H>
    void suspend(YieldContext<H> yield);
    /// @}

    /// @name Deleted Functions
    /// @{
    /** The reset function is removed from the coroutine API.
        It is removed because its behavior in Session is to abort all pending
        operations without invoking their associated asynchronous completion
        handlers. Implementing this same behavior in CoroSession could result
        in hung coroutines (because the pending CoroSession operations might
        never return). */
    void reset() = delete;
    /// @}

protected:
    using Base::Base;

private:
    template <typename H, typename R>
    using CoroHandler =
        typename boost::asio::async_result<
            YieldContext<H>,
            void(AsyncResult<R>)>::completion_handler_type;

    template <typename TResult, typename TYieldContext, typename TDelegate>
    TResult run(TYieldContext&& yield, std::error_code* ec,
                TDelegate&& delegate);
};


//******************************************************************************
// CoroSession implementation
//******************************************************************************

//------------------------------------------------------------------------------
/** @copydetails Session::create(AnyExecutor, const Connector::Ptr&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    AnyExecutor exec,               /**< Executor with which to post all
                                         user-provided handlers. */
    const Connector::Ptr& connector /**< Connection details for the transport
                                         to use. */
    )
{
    return Ptr(new CoroSession(exec, {connector}));
}

//------------------------------------------------------------------------------
/** @copydetails Session::create(AnyExecutor, const ConnectorList&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    AnyExecutor exec,               /**< Executor with which to post all
                                         user-provided handlers. */
    const ConnectorList& connectors /**< A list of connection details for
                                         the transports to use. */
    )
{
    return Ptr(new CoroSession(exec, connectors));
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
/** @copydetails Session::leave(AsyncHandler<Reason>)
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Reason CoroSession<B>::leave(
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
                                instead of throwing an exception upon failure. */
    )
{
    return leave(Reason("wamp.close.close_realm"), yield, ec);
}

//------------------------------------------------------------------------------
/** @copydetails Session::leave(Reason, AsyncHandler<Reason>)
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
/** @copydetails Session::enroll
    @throws error::Failure with an error code if a runtime error occured and
            the `ec` parameter is null. */
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
Registration CoroSession<B>::enroll(
    Procedure procedure,   /**< The procedure URI to register. */
    CallSlot callSlot,     /**< Callable target to invoke when a matching RPC
                                invocation is received. */
    InterruptSlot interruptSlot, /**< Handler to execute when RPC
                                      is interrupted. */
    YieldContext<H> yield, /**< Represents the current coroutine. */
    std::error_code* ec    /**< Optional pointer to an error code to set,
                                instead of throwing an exception upon
                                failure. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");

    return run<Registration>(yield, ec,
        [this, &procedure, &callSlot, &interruptSlot]
        (CoroHandler<H, Registration>& handler)
        {
            this->enroll(std::move(procedure), std::move(callSlot),
                         std::move(interruptSlot), handler);
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
```
boost::asio::post(this->userIosvc(), yield);
```
*/
//------------------------------------------------------------------------------
template <typename B>
template <typename H>
void CoroSession<B>::suspend(YieldContext<H> yield)
{
    boost::asio::post(this->userIosvc(), yield);
}

//------------------------------------------------------------------------------
template <typename B>
template <typename TResult, typename TYieldContext, typename TDelegate>
TResult CoroSession<B>::run(TYieldContext&& yield, std::error_code* ec,
                            TDelegate&& delegate)
{
    using Handler = typename boost::asio::async_result<
        std::decay_t<TYieldContext>,
        void(AsyncResult<TResult>)>::completion_handler_type;

    Handler handler(std::forward<TYieldContext>(yield));

    using ResultType =
        boost::asio::async_result<std::decay_t<TYieldContext>,
                                  void(AsyncResult<TResult>)>;

    ResultType result(handler);

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

#endif // CPPWAMP_CORO_COROSESSION_HPP
