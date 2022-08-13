/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2018, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CORO_COROSESSION_HPP
#define CPPWAMP_CORO_COROSESSION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Backward compatilibity header: use Session with completion tokens
           instead. */
//------------------------------------------------------------------------------

#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include "../asyncresult.hpp"
#include "../config.hpp"
#include "../session.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Coroutine API used by a _client_ peer in WAMP applications.

@deprecated Use wamp::Session with completion tokens instead.

This class differs from Session as follows:
- Only callback handler functions or [boost::asio::yield_context][yieldcontext]
  can be used as completion tokens.
- The results of coroutine operations are returned directly by the function,
  instead of via an ErrorOr object.
- Runtime errors are thrown as error::Failure exceptions.
- An optional pointer to a `std::error_code` can be passed to coroutine
  operations. If a runtime error occurs, it will set the pointed-to
  error code instead of throwing an error::Failure exception.

@par Aborting Coroutine Operations
All pending coroutine operations can be _aborted_ by dropping the client
connection via Session::disconnect. Pending post-join operations can be also
be aborted via CoroSession::leave. Operations aborted in this manner will
throw an error::Failure exception. There is currently no way to abort a
single operation via this class without dropping the connection or leaving
the realm.

@par Mixins
The mixin feature where this class can be combined with other Session-like
classes has been disabled. This class' template parameter is now ignored and
it now behaves as if it were mixed in with Session.

@tparam TBase Ignored
@extends Session
@see Session, Registration, Subscription. */
//------------------------------------------------------------------------------
template <typename TIgnoredBase = Session>
class CPPWAMP_DEPRECATED CoroSession: public Session
{
public:
    /** Shared pointer to a CoroSession. */
    using Ptr = std::shared_ptr<CoroSession>;

    /** The base class type that this mixin extends. */
    using Base = Session;

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
    static Ptr create(AnyIoExecutor exec, const Connector::Ptr& connector);

    /** Creates a new CoroSession instance. */
    static Ptr create(AnyIoExecutor exec, const ConnectorList& connectors);

    /** Creates a new CoroSession instance.
        @copydetails Session::create(AnyIoExecutor, const Connector::Ptr&)
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
        @copydetails Session::create(AnyIoExecutor, const Connector::Ptr&)
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

    /// @name Session Management
    /// @{
    /** Asynchronously attempts to connect to a router. */
    void connect(AsyncHandler<size_t> handler);

    /** Attempts to connect to a router. */
    template <typename H>
    size_t connect(YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Asynchronously attempts to join the given WAMP realm. */
    void join(Realm realm, AsyncHandler<SessionInfo> handler);

    /** Attempts to join the given WAMP realm. */
    template <typename H>
    SessionInfo join(Realm realm, YieldContext<H> yield,
                     std::error_code* ec = nullptr);

    /** Sends an `AUTHENTICATE` in response to a `CHALLENGE`. */
    void authenticate(Authentication auth);

    /** Asynchronously leaves the WAMP session. */
    void leave(AsyncHandler<Reason> handler);

    /** Leaves the WAMP session. */
    template <typename H>
    Reason leave(YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Asynchronously leaves the WAMP session with the given reason. */
    void leave(Reason reason, AsyncHandler<Reason> handler);

    /** Leaves the WAMP session with the given reason. */
    template <typename H>
    Reason leave(Reason reason, YieldContext<H> yield,
                 std::error_code* ec = nullptr);
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Asynchronously subscribes to WAMP pub/sub events having the given
        topic. */
    void subscribe(Topic topic, EventSlot slot,
                   AsyncHandler<Subscription> handler);

    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename H>
    Subscription subscribe(Topic topic, EventSlot slot,
            YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Unsubscribes a subscription to a topic. */
    void unsubscribe(const Subscription& sub);

    /** Asynchronously unsubscribes a subscription to a topic and waits for
        router acknowledgement, if necessary. */
    void unsubscribe(const Subscription& sub, AsyncHandler<bool> handler);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement if necessary. */
    template <typename H>
    bool unsubscribe(const Subscription& sub, YieldContext<H> yield,
                     std::error_code* ec = nullptr);

    /** Publishes an event. */
    void publish(Pub pub);

    /** Asynchronously publishes an event and waits for an acknowledgement from
        the router. */
    void publish(Pub pub, AsyncHandler<PublicationId> handler);

    /** Publishes an event and waits for an acknowledgement from the router. */
    template <typename H>
    PublicationId publish(Pub pub, YieldContext<H> yield,
                          std::error_code* ec = nullptr);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Asynchronously registers a WAMP remote procedure call. */
    void enroll(Procedure procedure, CallSlot slot,
                AsyncHandler<Registration> handler);

    /** Registers a WAMP remote procedure call. */
    template <typename H>
    Registration enroll(Procedure procedure, CallSlot slot,
            YieldContext<H> yield, std::error_code* ec = nullptr);

    /** Asynchronously registers a WAMP remote procedure call with an
        interruption handler. */
    void enroll(Procedure procedure, CallSlot callSlot,
                InterruptSlot interruptSlot,
                AsyncHandler<Registration> handler);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    template <typename H>
    Registration enroll(Procedure procedure, CallSlot slot,
                        InterruptSlot interruptSlot, YieldContext<H> yield,
                        std::error_code* ec = nullptr);

    /** Unregisters a remote procedure call. */
    void unregister(const Registration& reg);

    /** Asynchronously unregisters a remote procedure call and waits for router
        acknowledgement. */
    void unregister(const Registration& reg, AsyncHandler<bool> handler);

    /** Unregisters a remote procedure call and waits for router
        acknowledgement. */
    template <typename H>
    bool unregister(const Registration& reg, YieldContext<H> yield,
                    std::error_code* ec = nullptr);

    /** Asynchronously calls a remote procedure. */
    RequestId call(Rpc procedure, AsyncHandler<Result> handler);

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
};


//******************************************************************************
// CoroSession implementation
//******************************************************************************

//------------------------------------------------------------------------------
/** @copydetails Session::create(AnyIoExecutor, const Connector::Ptr&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    AnyIoExecutor exec,             /**< Executor with which to post all
                                         user-provided handlers. */
    const Connector::Ptr& connector /**< Connection details for the transport
                                         to use. */
    )
{
    return Ptr(new CoroSession(exec, {connector}));
}

//------------------------------------------------------------------------------
/** @copydetails Session::create(AnyIoExecutor, const ConnectorList&) */
//------------------------------------------------------------------------------
template <typename B>
typename CoroSession<B>::Ptr CoroSession<B>::create(
    AnyIoExecutor exec,             /**< Executor with which to post all
                                         user-provided handlers. */
    const ConnectorList& connectors /**< A list of connection details for
                                         the transports to use. */
    )
{
    return Ptr(new CoroSession(exec, connectors));
}

//------------------------------------------------------------------------------
/** @details
    The session will attempt to connect using the transports that were
    specified by the wamp::Connector objects passed during create().
    If more than one transport was specified, they will be traversed in the
    same order as they appeared in the @ref ConnectorList.
    @return The index of the Connector object used to establish the connetion.
    @pre `this->state() == SessionState::disconnected`
    @post `this->state() == SessionState::connecting`
    @par Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - SessionErrc::allTransportsFailed if more than one transport was
          specified and they all failed to connect.
        - Some other platform or transport-dependent `std::error_code` if
          only one transport was specified and it failed to connect.
    @throws error::Logic if `this->state() != SessionState::disconnected` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::connect(
    AsyncHandler<size_t> handler /**< Handler to invoke when the operation
                                      completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(state() == State::disconnected,
                        "Session is not disconnected");
    Base::connect(std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::connect(AsyncHandler<size_t>)
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
    CPPWAMP_LOGIC_CHECK(state() == State::disconnected,
                        "Session is not disconnected");
    auto index = Base::connect(yield);
    if (!ec)
        return index.value();
    *ec = !index ? index.error() : std::error_code{};
    return index.value_or(0);
}

//------------------------------------------------------------------------------
/** @return A SessionInfo object with details on the newly established session.
    @pre `this->state() == SessionState::connected`
    @post `this->state() == SessionState::establishing`
    @par Error Codes
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchRealm if the realm does not exist.
        - SessionErrc::noSuchRole if one of the client roles is not supported on
          the router.
        - SessionErrc::joinError for other errors reported by the router.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::connected` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::join(
    Realm realm,                      /**< Details on the realm to join. */
    AsyncHandler<SessionInfo> handler /**< Handler to invoke when the
                                           operation completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::closed,
                        "Session is not closed");
    Base::join(std::move(realm), std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::join(Realm, AsyncHandler<SessionInfo>)
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
    auto info = Base::join(std::move(realm), yield);
    if (!ec)
        return info.value();
    *ec = !info ? info.error() : std::error_code{};
    return info.value_or(SessionInfo{});
}

//------------------------------------------------------------------------------
/** @pre `this->state() == SessionState::authenticating`
    @throw error::Logic if `this->state() != SessionState::authenticating` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::authenticate(Authentication auth)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::authenticating,
                        "Session is not authenticating");
    Base::authenticate(std::move(auth));
}

//------------------------------------------------------------------------------
/** @details The "wamp.close.close_realm" reason is sent as part of the
             outgoing `GOODBYE` message.
    @return The _Reason_ URI and details from the `GOODBYE` response returned
            by the router.
    @pre `this->state() == SessionState::established`
    @post `this->state() == SessionState::shuttingDown`
    @par Error Codes
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer
          before a `GOODBYE` response was received.
        - Some other `std::error_code` for protocol and transport errors.
    @throw error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::leave(
    AsyncHandler<Reason> handler /**< Handler to invoke when the
                                      operation completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::leave(std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::leave(AsyncHandler<Reason>)
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
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    auto reason = Base::leave(yield);
    if (!ec)
        return reason.value();
    *ec = !reason ? reason.error() : std::error_code{};
    return reason.value_or(Reason{});
}

//------------------------------------------------------------------------------
/** @return The _Reason_ URI and details from the `GOODBYE` response returned
            by the router.
    @pre `this->state() == SessionState::established`
    @post `this->state() == SessionState::shuttingDown`
    @par Error Codes
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer
          before a `GOODBYE` response was received.
        - Some other `std::error_code` for protocol and transport errors.
    @throw error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::leave(
    Reason reason,               /**< %Reason URI and other options */
    AsyncHandler<Reason> handler /**< Handler to invoke when the
                                      operation completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::leave(std::move(reason), std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::leave(Reason, AsyncHandler<Reason>)
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
    auto result = Base::leave(std::move(reason), yield);
    if (!ec)
        return result.value();
    *ec = !result ? result.error() : std::error_code{};
    return result.value_or(Reason{});
}

//------------------------------------------------------------------------------
/** @see @ref Subscriptions

    @return A Subscription object, therafter used to manage the subscription's
            lifetime.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::subscribeError if the router replied with an `ERROR`
          response.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::subscribe(
    Topic topic,    /**< The topic to subscribe to. */
    EventSlot slot, /**< The callable target to invoke when a matching
                         event is received. */
    AsyncHandler<Subscription> handler /**< Handler to invoke when the
                                            subscribe operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    using std::move;
    return Base::subscribe(move(topic), move(slot), move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::subscribe(Topic, EventSlot, AsyncHandler<Subscription>)
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
    auto sub = Base::subscribe(std::move(topic), std::move(slot), yield);
    if (!ec)
        return sub.value();
    *ec = !sub ? sub.error() : std::error_code{};
    return sub.value_or(Subscription{});
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any session state. If the
    subscription is no longer applicable, then the unsubscribe operation
    will effectively do nothing.
    @see Subscription, ScopedSubscription
    @note Duplicate unsubscribes using the same Subscription object
          are safely ignored.
    @pre `!!sub == true`
    @throws error::Logic if the given subscription is empty */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::unsubscribe(
    const Subscription& sub /**< The subscription to unsubscribe from. */
    )
{
    Base::unsubscribe(sub);
}

//------------------------------------------------------------------------------
/** @details
    If there are other local subscriptions on this session remaining for the
    same topic, then the session does not send an `UNSUBSCRIBE` message to
    the router.
    @see Subscription, ScopedSubscription
    @returns `false` if the subscription was already removed, `true` otherwise.
    @note Duplicate unsubscribes using the same Subscription handle
          are safely ignored.
    @pre `!!sub == true`
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchSubscription if the router reports that there was
          no such subscription.
        - SessionErrc::unsubscribeError if the router reports some other
          error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if the given subscription is empty
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::unsubscribe(
    const Subscription& sub,   /**< The subscription to unsubscribe from. */
    AsyncHandler<bool> handler /**< Handler to invoke when the operation
                                    completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::unsubscribe(sub, std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::unsubscribe(const Subscription&, AsyncHandler<bool>)
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
    auto unsubscribed = Base::unsubscribe(sub, yield);
    if (!ec)
        return unsubscribed.value();
    *ec = !unsubscribed ? unsubscribed.error() : std::error_code{};
    return unsubscribed.value_or(false);
}

//------------------------------------------------------------------------------
/** @pre `this->state() == SessionState::established`
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::publish(
    Pub pub /**< The publication to publish. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::publish(pub);
}

//------------------------------------------------------------------------------
/** @return The publication ID for this event.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::publishError if the router replies with an ERROR
          response.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::publish(
    Pub pub,                            /**< The publication to publish. */
    AsyncHandler<PublicationId> handler /**< Handler to invoke when
                                             the operation completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::publish(pub, std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::publish(Pub, AsyncHandler<PublicationId>)
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
    auto pubId = Base::publish(std::move(pub), yield);
    if (!ec)
        return pubId.value();
    *ec = !pubId ? pubId.error() : std::error_code{};
    return pubId.value_or(0);
}

//------------------------------------------------------------------------------
/** @see @ref Registrations

    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::procedureAlreadyExists if the router reports that the
          procedure has already been registered for this realm.
        - SessionErrc::registerError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::enroll(
    Procedure procedure, /**< The procedure to register. */
    CallSlot slot,       /**< The handler to execute when the RPC is invoked. */
    AsyncHandler<Registration> handler /**< Handler to invoke when
                                            the enroll operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::enroll(std::move(procedure), std::move(slot), std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::enroll(Procedure, CallSlot, AsyncHandler<Registration>)
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
    auto reg = Base::enroll(std::move(procedure), std::move(slot), yield);
    if (!ec)
        return reg.value();
    *ec = !reg ? reg.error() : std::error_code{};
    return reg.value_or(Registration{});
}

//------------------------------------------------------------------------------
/** @see @ref Registrations

    @return A Registration object, therafter used to manage the registration's
            lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::procedureAlreadyExists if the router reports that the
          procedure has already been registered for this realm.
        - SessionErrc::registerError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::enroll(
    Procedure procedure, /**< The procedure to register. */
    CallSlot callSlot,   /**< The handler to execute when the RPC is invoked. */
    InterruptSlot interruptSlot, /**< Handler to execute when RPC
                                      is interrupted. */
    AsyncHandler<Registration> handler /**< Handler to invoke when
                                            the enroll operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::enroll(std::move(procedure), std::move(callSlot),
                 std::move(interruptSlot), std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::enroll(Procedure, CallSlot, InterruptSlot, AsyncHandler<Registration>)
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
    auto reg = Base::enroll(std::move(procedure), std::move(callSlot),
                            std::move(interruptSlot), yield);
    if (!ec)
        return reg.value();
    *ec = !reg ? reg.error() : std::error_code{};
    return reg.value_or(Registration{});
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any session state. If the
    registration is no longer applicable, then the unregister operation
    will effectively do nothing.
    @see Registration, ScopedRegistration
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `!!reg == true`
    @throws error::Logic if the given registration is empty */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::unregister(
    const Registration& reg /**< The RPC registration to unregister. */
    )
{
    Base::unregister(reg);
}

//------------------------------------------------------------------------------
/** @see Registration, ScopedRegistration
    @returns `false` if the registration was already removed, `true` otherwise.
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `!!reg == true`
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchRegistration if the router reports that there is
          no such procedure registered by that name.
        - SessionErrc::unregisterError if the router reports some other
          error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if the given registration is empty
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
void CoroSession<B>::unregister(
    const Registration& reg,   /**< The RPC registration to unregister. */
    AsyncHandler<bool> handler /**< Handler to invoke when the operation
                                    completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    Base::unregister(reg, std::move(handler));
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::unregister(const Registration&, AsyncHandler<bool>)
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
    auto unregistered = Base::unregister(reg, yield);
    if (!ec)
        return unregistered.value();
    *ec = !unregistered ? unregistered.error() : std::error_code{};
    return unregistered.value_or(false);
}

//------------------------------------------------------------------------------
/** @return The Result yielded by the remote procedure
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - SessionErrc::sessionEnded if the operation was aborted.
        - SessionErrc::sessionEndedByPeer if the session was ended by the peer.
        - SessionErrc::noSuchProcedure if the router reports that there is
          no such procedure registered by that name.
        - SessionErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - SessionErrc::callError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename B>
RequestId CoroSession<B>::call(
    Rpc rpc,                     /**< Details about the RPC. */
    AsyncHandler<Result> handler /**< Handler to invoke when the
                                       operation completes. */
    )
{
    CPPWAMP_LOGIC_CHECK(this->state() == State::established,
                        "Session is not established");
    CallChit chit;
    Base::call(std::move(rpc), chit, std::move(handler));
    return chit.requestId();
}

//------------------------------------------------------------------------------
/** @copydetails CoroSession::call(Rpc, AsyncHandler<Result>)
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
    auto result = Base::call(std::move(rpc), yield);
    if (!ec)
        return result.value();
    *ec = !result ? result.error() : std::error_code{};
    return result.value_or(Result{});
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

} // namespace wamp

#endif // CPPWAMP_CORO_COROSESSION_HPP
