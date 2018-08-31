/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COROSESSION_HPP
#define CPPWAMP_COROSESSION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the coroutine-based API used by a _client_ peer in WAMP
    applications. */
//------------------------------------------------------------------------------

#include <boost/asio/spawn.hpp>
#include "session.hpp"

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
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void join(std::string realm, AsyncHandler<SessionInfo> handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    becomes the following equivalent coroutine operation:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    SessionInfo join(std::string realm, YieldContext<H> yield,
                     std::error_code* ec = nullptr);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    A yield context is obtained via [boost::asio::spawn][spawn]. For example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    boost::asio::io_service iosvc;
    auto session = CoroSession<>::create(connectorList);
    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        session->connect(yield);
        SessionInfo info = session->join("somerealm", yield);
        // etc...
    });
    iosvc.run();
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Mixin both the CoroSession and FutuSession APIs:
    using SessionApi = FutuSession<CoroSession<>>;
    auto session = SessionApi::create(connectorList);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

    /** Yield context type used by the boost::asio::spawn handler. */
    template <typename TSpawnHandler>
    using YieldContext = boost::asio::basic_yield_context<TSpawnHandler>;

    /** Creates a new CoroSession instance. */
    static Ptr create(AsioService& userIosvc, const Connector::Ptr& connector);

    /** Creates a new CoroSession instance. */
    static Ptr create(AsioService& userIosvc, const ConnectorList& connectors);

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
    using CoroHandler = typename boost::asio::handler_type<
                            YieldContext<H>,
                            void(AsyncResult<R>)
                        >::type;

    template <typename TResult, typename TYieldContext, typename TDelegate>
    TResult run(TYieldContext&& yield, std::error_code* ec,
                TDelegate&& delegate);
};

} // namespace wamp

#include "internal/corosession.ipp"

#endif // CPPWAMP_COROSESSION_HPP
