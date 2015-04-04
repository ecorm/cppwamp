/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COROERRCCLIENT_HPP
#define CPPWAMP_COROERRCCLIENT_HPP

//------------------------------------------------------------------------------
/** @file
    Contains an alternate coroutine-based API used by a _client_ peer in WAMP
    applications. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <boost/asio/spawn.hpp>
#include "client.hpp"
#include "connector.hpp"
#include "wampdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Alternate coroutine API used by a _client_ peer in WAMP applications.

    This mixin class adds a _coroutine_ API on top of the asynchronous one
    provided by Client. This class provides the same functionality as
    CoroClient, except that error codes are returned instead of error::wamp
    exceptions being thrown. This alternate interface is useful for client
    programs that prefer to deal with error codes instead of exceptions.

    @note error::Logic exceptions are still thrown whenever the preconditions
          of a member function are not met.

    Coroutines enable client programs to implement
    asynchronous logic in a synchronous manner. This class is based on the
    [stackful coroutines][asiocoro] implementation used by Boost.Asio.

    The asynchronous operations in Client are mapped to coroutine operations as
    follows:
    - A [boost::asio::yield_context][yieldcontext] is passed in place of the
      asynchronous completion handler.
    - The result is returned directly by the function, instead of via an
      AsyncResult object passed to the completion handler.
    - Errors codes, if applicable, are returned via "out" parameters.

    For example, the asynchronous operation,
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void join(std::string realm, AsyncHandler<SessionId> handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    becomes the following equivalent CoroErrcClient operation:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    SessionId join(std::string realm, YieldContext<H> yield, std::error_code& ec);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    A yield context is obtained via [boost::asio::spawn][spawn]. For example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    boost::asio::io_service iosvc;
    auto client = CoroErrcClient<>::create(connectorList);
    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        std::error_code ec;
        client->connect(yield, ec);
        assert(!ec);
        SessionId sid = client->join("somerealm", yield, ec);
        assert(!ec);
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
    be aborted via CoroErrcClient::leave. Operations aborted in this manner
    will return a non-zero error code. There is currently no way to abort a
    single operation without dropping the connection or leaving the realm.

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
    @see Client, CoroClient, Registration, Subscription. */
//------------------------------------------------------------------------------
template <typename TBase = Client>
class CoroErrcClient : public TBase
{
public:
    /** Shared pointer to a CoroErrcClient. */
    using Ptr = std::shared_ptr<CoroErrcClient>;

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
    size_t connect(YieldContext<H> yield, std::error_code& ec);

    /** Attempts to join the given WAMP realm. */
    template <typename H>
    SessionId join(std::string realm, YieldContext<H> yield,
                   std::error_code& ec);

    /** Leaves the given WAMP realm. */
    template <typename H>
    std::string leave(YieldContext<H> yield, std::error_code& ec);

    /** Leaves the given WAMP realm with a _Reason_ URI. */
    template <typename H>
    std::string leave(std::string reason, YieldContext<H> yield,
                      std::error_code& ec);
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename... TParams, typename TEventSlot, typename H>
    Subscription subscribe(std::string topic, TEventSlot&& slot,
                           YieldContext<H> yield, std::error_code& ec);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement if necessary. */
    template <typename H>
    void unsubscribe(Subscription sub, YieldContext<H> yield,
                     std::error_code& ec);

    /** Publishes an argumentless event with the given topic and waits
        for an acknowledgement from the router. */
    template <typename H>
    PublicationId publish(std::string topic, YieldContext<H> yield,
                          std::error_code& ec);

    /** Publishes an event with the given topic and argument values, and waits
        for an acknowledgement from the router. */
    template <typename H>
    PublicationId publish(std::string topic, Args args, YieldContext<H> yield,
                          std::error_code& ec);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    template <typename... TParams, typename TCallSlot, typename H>
    Registration enroll(std::string procedure, TCallSlot&& slot,
                        YieldContext<H> yield, std::error_code& ec);

    /** Unregisters a remote procedure call. */
    template <typename H>
    void unregister(Registration reg, YieldContext<H> yield,
                    std::error_code& ec);

    /** Calls an argumentless remote procedure call. */
    template <typename H>
    Args call(std::string procedure, YieldContext<H> yield,
              std::error_code& ec);

    /** Calls a remote procedure call with the given arguments. */
    template <typename H>
    Args call(std::string procedure, Args args, YieldContext<H> yield,
              std::error_code& ec);
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
        handlers. Implementing this same behavior in CoroErrcClient could
        result in hung coroutines (because the pending CoroErrcClient
        operations might never return). */
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
    TResult run(TYieldContext&& yield, std::error_code& ec,
                TDelegate&& delegate);
};

} // namespace wamp

#include "internal/coroerrcclient.ipp"

#endif // CPPWAMP_COROERRCCLIENT_HPP
