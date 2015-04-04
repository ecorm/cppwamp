/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COROCLIENT_HPP
#define CPPWAMP_COROCLIENT_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the coroutine-based API used by a _client_ peer in WAMP
    applications. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <boost/asio/spawn.hpp>
#include "client.hpp"
#include "connector.hpp"
#include "error.hpp"
#include "wampdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Coroutine API used by a _client_ peer in WAMP applications.

    This mixin class adds a _coroutine_ API on top of the asynchronous one
    provided by Client. Coroutines enable client programs to implement
    asynchronous logic in a synchronous manner. This class is based on the
    [stackful coroutines][asiocoro] implementation used by Boost.Asio.

    The asynchronous operations in Client are mapped to coroutine operations as
    follows:
    - A [boost::asio::yield_context][yieldcontext] is passed in place of the
      asynchronous completion handler.
    - The result is returned directly by the function, instead of via an
      AsyncResult object passed to the completion handler.
    - Runtime errors are thrown as error::Wamp exceptions. When caught, the
      error code can be retrieved via `error::Wamp::code`.

    For example, the asynchronous operation,
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void join(std::string realm, AsyncHandler<SessionId> handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    becomes the following equivalent coroutine operation:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    SessionId join(std::string realm, YieldContext<H> yield);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    A yield context is obtained via [boost::asio::spawn][spawn]. For example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    boost::asio::io_service iosvc;
    auto client = CoroClient<>::create(connectorList);
    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        client->connect(yield);
        SessionId sid = client->join("somerealm", yield);
        // etc...
    });
    iosvc.run();
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    [asiocoro]: http://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/spawn.html
    [yieldcontext]: http://www.boost.org/doc/libs/1_57_0/doc/html/boost_asio/reference/yield_context.html
    [spawn]: http://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/spawn.html

    @par Aborting Coroutine Operations
    All pending coroutine operations can be _aborted_ by dropping the client
    connection via Client::disconnect. Pending post-join operations can be also
    be aborted via CoroClient::leave. Operations aborted in this manner will
    throw an error::Wamp exception. There is currently no way to abort a single
    operation without dropping the connection or leaving the realm.

    @par Mixins
    This mixin class can be combined with other client mixin classes,
    by chaining the `TBase` template parameter. For example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Mixin both the CoroClient and CoroErrcClient APIs:
    using ClientApi = CoroErrcClient<CoroClient<>>;
    auto client = ClientApi::create(connectorList);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    @tparam TBase The base class that this mixin extends.
    @extends Client
    @see Client, CoroErrcClient, Registration, Subscription. */
//------------------------------------------------------------------------------
template <typename TBase = Client>
class CoroClient : public TBase
{
public:
    /** Shared pointer to a CoroClient. */
    using Ptr = std::shared_ptr<CoroClient>;

    /** The base class type that this mixin extends. */
    using Base = TBase;

    /** Enumerates the possible states that a CoroClient can be in. */
    using State = SessionState;

    /** Yield context type used by the boost::asio::spawn handler. */
    template <typename TSpawnHandler>
    using YieldContext = boost::asio::basic_yield_context<TSpawnHandler>;

    /** Creates a new CoroClient instance. */
    static Ptr create(const Connector::Ptr& connector);

    /** Creates a new CoroClient instance. */
    static Ptr create(const ConnectorList& connectors);

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

    /// @name Session Management
    /// @{
    /** Attempts to connect to a router. */
    template <typename H>
    size_t connect(YieldContext<H> yield);

    /** Attempts to join the given WAMP realm. */
    template <typename H>
    SessionId join(std::string realm, YieldContext<H> yield);

    /** Leaves the given WAMP realm. */
    template <typename H>
    std::string leave(YieldContext<H> yield);

    /** Leaves the given WAMP realm with a _Reason_ URI. */
    template <typename H>
    std::string leave(std::string reason, YieldContext<H> yield);
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename... TParams, typename TEventSlot, typename H>
    Subscription subscribe(std::string topic, TEventSlot&& slot,
                           YieldContext<H> yield);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement if necessary. */
    template <typename H>
    void unsubscribe(Subscription sub, YieldContext<H> yield);

    /** Publishes an argumentless event with the given topic and waits
        for an acknowledgement from the router. */
    template <typename H>
    PublicationId publish(std::string topic, YieldContext<H> yield);

    /** Publishes an event with the given topic and argument values, and waits
        for an acknowledgement from the router. */
    template <typename H>
    PublicationId publish(std::string topic, Args args, YieldContext<H> yield);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    template <typename... TParams, typename TCallSlot, typename H>
    Registration enroll(std::string procedure, TCallSlot&& slot,
                        YieldContext<H> yield);

    /** Unregisters a remote procedure call. */
    template <typename H>
    void unregister(Registration reg, YieldContext<H> yield);

    /** Calls an argumentless remote procedure call. */
    template <typename H>
    Args call(std::string procedure, YieldContext<H> yield);

    /** Calls a remote procedure call with the given arguments. */
    template <typename H>
    Args call(std::string procedure, Args args, YieldContext<H> yield);
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
        It is removed because its behavior in client is to abort all pending
        operations without invoking their associated asynchronous completion
        handlers. Implementing this same behavior in CoroClient could result
        in hung coroutines (because the pending CoroClient operations might
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
    TResult run(TYieldContext&& yield, TDelegate&& delegate);
};

} // namespace wamp

#include "internal/coroclient.ipp"

#endif // CPPWAMP_COROCLIENT_HPP
