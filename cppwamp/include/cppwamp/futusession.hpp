/*------------------------------------------------------------------------------
                     Copyright Emile Cormier 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_FUTUSESSION_HPP
#define CPPWAMP_FUTUSESSION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the futures-based API used by a _client_ peer in WAMP
    applications. */
//------------------------------------------------------------------------------

#ifndef BOOST_THREAD_VERSION
#define BOOST_THREAD_VERSION 4
#endif

#include <boost/thread/future.hpp>
#include "session.hpp"

namespace wamp
{

/** Empty tag type used to distinguish FutuSession overloads from the ones
    in Session. */
struct WithFuture {};

/** Empty tag object used to distinguish FutuSession overloads from the ones
    in Session. */
constexpr WithFuture withFuture = WithFuture();

//------------------------------------------------------------------------------
/** Experimental future-based API for WAMP client applications.

    This mixin class adds a future-based API on top of the asynchronous one
    provided by Session. Futures provide a way to retrieve the result of an
    asynchronous operation. This class uses [boost::future][boost_future],
    which provides `.then()` continuations.

    The asynchronous operations in Session are mapped to FutuSession operations
    as follows:
    - Wherever a Session operation expects an AsyncHandler<T> parameter,
      a `boost::future<T>` is returned instead by FutuSession.
    - Runtime errors are thrown as exceptions that are transported to
      `boost::future:get()`.

    For example, the asynchronous operation,
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void join(std::string realm, AsyncHandler<SessionInfo> handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    becomes the following equivalent FutuSession operation:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    boost::future<SessionInfo> join(std::string realm);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    [boost_future]:
        http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.futures

    @par Continuations
    Write me!

    @warning <table><tr><td><b>
             [%boost::future::then][boost_future_then] continuations are still
             experimental (as of Boost version 1.58), and are subject to change.
             </b></td></tr></table>

    [boost_future_then]:
        http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.futures.reference.unique_future.then

    @par Aborting Future Operations
    All pending future operations can be _aborted_ by dropping the client
    connection via Session::disconnect. Pending post-join operations can be also
    be aborted via FutuSession::leave. Operations aborted in this manner will
    throw an error::Failure exception. There is currently no way to abort a
    single operation without dropping the connection or leaving the realm.

    @par Terminating Asynchronous Operations
    All pending future operations can be _terminated_ by dropping the
    client connection via Session::reset or the Session destructor. By design,
    the handlers for pending operations will not be invoked if they
    were terminated in this way. This is useful if a client application needs
    to shutdown abruptly and cannot enforce the lifetime of objects accessed
    within the asynchronous operation handlers.

    @par Mixins
    This mixin class can be combined with other session mixin classes,
    by chaining the `TBase` template parameter. For example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Mixin both the CoroSession and FutuSession APIs:
    using SessionApi = FutuSession<CoroSession<>>;
    auto session = SessionApi::create(connectorList);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    @tparam TBase The base class that this mixin extends.
    @extends Session
    @see Session, CoroSession, Registration, Subscription. */
//------------------------------------------------------------------------------
template <typename TBase = Session>
class FutuSession : public TBase
{
public:
    /** Shared pointer to a CoroSession. */
    using Ptr = std::shared_ptr<FutuSession>;

    /** The base class type that this mixin extends. */
    using Base = TBase;

    /** Alias to boost::future. */
    template <typename R> using Future = boost::future<R>;

    /** Enumerates the possible states that a FutuSession can be in. */
    using State = SessionState;

    /** Function type for handling pub/sub events. */
    using EventSlot = std::function<void (Event)>;

    /** Function type for handling remote procedure calls. */
    using CallSlot = std::function<Outcome (Invocation)>;

    /** Creates a new FutuSession instance. */
    static Ptr create(const Connector::Ptr& connector);

    /** Creates a new FutuSession instance. */
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
    Future<size_t> connect();

    /** Attempts to join the given WAMP realm. */
    Future<SessionInfo> join(Realm realm);

    /** Leaves the WAMP session. */
    Future<Reason> leave(Reason reason);
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    Future<Subscription> subscribe(Topic topic, EventSlot slot);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement if necessary. */
    Future<bool> unsubscribe(const Subscription& sub, WithFuture);

    /** Publishes an event and waits for an acknowledgement from the router. */
    Future<PublicationId> publish(Pub pub, WithFuture);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    Future<Registration> enroll(Procedure procedure, CallSlot slot);

    /** Unregisters a remote procedure call and waits for an acknowledgement
        from the router. */
    Future<bool> unregister(const Registration& reg, WithFuture);

    /** Calls a remote procedure */
    Future<Result> call(Rpc rpc);
    /// @}

protected:
    using Base::Base;

private:
    template <typename TResult, typename TDelegate>
    Future<TResult> run(TDelegate delegate);
};


} // namespace wamp

#include "internal/futusession.ipp"

#endif // CPPWAMP_FUTUSESSION_HPP
