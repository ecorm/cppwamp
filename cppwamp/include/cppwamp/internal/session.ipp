/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <cassert>
#include <iostream>
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** @post `session->state() == SessionState::disconnected`
    @return A shared pointer to the created session object. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    AsioService& userIosvc,         /**< IO service in which to post all
                                         user-provided handlers. */
    const Connector::Ptr& connector /**< Connection details for the transport
                                         to use. */
)
{
    return Ptr(new Session(userIosvc, {connector}));
}

//------------------------------------------------------------------------------
/** @pre `connectors.empty() == false`
    @post `session->state() == SessionState::disconnected`
    @return A shared pointer to the created Session object.
    @throws error::Logic if `connectors.empty() == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    AsioService& userIosvc,         /**< IO service in which to post all
                                         user-provided handlers. */
    const ConnectorList& connectors /**< A list of connection details for
                                         the transports to use. */
)
{
    return Ptr(new Session(userIosvc, connectors));
}

//------------------------------------------------------------------------------
/** @details
    The dictionary is structured as per `HELLO.Details.roles`, as desribed in
    the ["Client: Role and Feature Announcement"][feature-announcement]
    section of the advanced WAMP specification.

    [feature-announcement]: https://tools.ietf.org/html/draft-oberstet-hybi-tavendo-wamp-02#section-7.1.1.1 */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const Object& Session::roles()
{
    return internal::ClientInterface::roles();
}

//------------------------------------------------------------------------------
/** @details
    Automatically invokes reset() on the session, which drops the
    connection and terminates all pending asynchronous operations. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::~Session()
{
    reset();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AsioService& Session::userIosvc() const
{
    return userIosvc_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionState Session::state() const
{
    return impl_ ? impl_->state() : state_;
}

//------------------------------------------------------------------------------
/** @details
    Warnings occur when the session encounters problems that do not prevent
    it from proceeding normally. An example of such warnings is when a
    peer attempts to send an event with arguments that does not match the types
    of a statically-typed event slot.

    By default, warnings are logged to std::cerr.

    @note Changes to the warning handler start taking effect when the session
          is connecting. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setWarningHandler(
    LogHandler handler /**< Function called to handle warnings. */
)
{
    warningHandler_ = AsyncTask<std::string>
    {
        userIosvc_,
        [handler](AsyncResult<std::string> warning) {handler(warning.get());}
    };
}

//------------------------------------------------------------------------------
/** @details
    By default, debug traces are discarded.

    @note Changes to the trace handler start taking effect when the session is
          connecting. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setTraceHandler(
    LogHandler handler /**< Function called to handle log traces. */
)
{
    traceHandler_ = AsyncTask<std::string>
    {
        userIosvc_,
        [handler](AsyncResult<std::string> trace) {handler(trace.get());}
    };
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setChallengeHandler(
    ChallengeHandler handler /**< Function called to handle
                                  authentication challenges. */
)
{
    challengeHandler_ = AsyncTask<Challenge>
    {
        userIosvc_,
        [handler](AsyncResult<Challenge> challenge)
        {
            handler(std::move(challenge.get()));
        }
    };
}

//------------------------------------------------------------------------------
/** @details
    The session will attempt to connect using the transports that were
    specified by the Connector objects passed during create().
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
CPPWAMP_INLINE void Session::connect(
    AsyncHandler<size_t> handler /**< Handler to invoke when the operation
                                      completes. */
)
{
    assert(!connectors_.empty());
    CPPWAMP_LOGIC_CHECK(state() == State::disconnected,
                        "Session is not disconnected");
    state_ = State::connecting;
    isTerminating_ = false;
    currentConnector_ = nullptr;
    doConnect(0, {userIosvc_, handler});
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
CPPWAMP_INLINE void Session::join(
    Realm realm,                      /**< Details on the realm to join. */
    AsyncHandler<SessionInfo> handler /**< Handler to invoke when the
                                           operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::closed, "Session is not closed");
    impl_->join(std::move(realm), {userIosvc_, std::move(handler)});
}

//------------------------------------------------------------------------------
/** @pre `this->state() == SessionState::authenticating`
    @throw error::Logic if `this->state() != SessionState::authenticating` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::authenticate(Authentication auth)
{
    CPPWAMP_LOGIC_CHECK(state() == State::authenticating,
                        "Session is not authenticating");
    impl_->authenticate(std::move(auth));
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
CPPWAMP_INLINE void Session::leave(
    Reason reason,               /**< %Reason URI and other options */
    AsyncHandler<Reason> handler /**< Handler to invoke when the
                                      operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->leave(std::move(reason), {userIosvc_, std::move(handler)});
}

//------------------------------------------------------------------------------
/** @details
    Aborts all pending asynchronous operations, invoking their handlers
    with error codes indicating that cancellation has occured.
    @post `this->state() == SessionState::disconnected` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::disconnect()
{
    bool isConnecting = state() == State::connecting;
    state_ = State::disconnected;
    if (isConnecting)
    {
        currentConnector_->cancel();
    }
    else if (impl_)
    {
        state_ = State::disconnected;
        impl_->disconnect();
        impl_.reset();
    }
}

//------------------------------------------------------------------------------
/** @details
    Terminates all pending asynchronous operations, which does **not**
    invoke their handlers. This is useful when a client application needs
    to shutdown abruptly and cannot enforce the lifetime of objects
    accessed within the asynchronous operation handlers.
    @post `this->state() == SessionState::disconnected` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::reset()
{
    bool isConnecting = state() == State::connecting;
    isTerminating_ = true;
    state_ = State::disconnected;
    if (isConnecting)
    {
        currentConnector_->cancel();
    }
    else if (impl_)
    {
        impl_->terminate();
        impl_.reset();
    }
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
CPPWAMP_INLINE void Session::subscribe(
    Topic topic,    /**< The topic to subscribe to. */
    EventSlot slot, /**< The callable target to invoke when a matching
                         event is received. */
    AsyncHandler<Subscription> handler /**< Handler to invoke when the
                                            subscribe operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    using std::move;
    impl_->subscribe(move(topic), move(slot), {userIosvc_, move(handler)});
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
CPPWAMP_INLINE void Session::unsubscribe(
    const Subscription& sub /**< The subscription to unsubscribe from. */
)
{
    CPPWAMP_LOGIC_CHECK(!!sub, "The subscription is empty");
    if (impl_)
        impl_->unsubscribe(sub);
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
CPPWAMP_INLINE void Session::unsubscribe(
    const Subscription& sub,   /**< The subscription to unsubscribe from. */
    AsyncHandler<bool> handler /**< Handler to invoke when the operation
                                    completes. */
)
{
    CPPWAMP_LOGIC_CHECK(!!sub, "The subscription is empty");
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->unsubscribe(sub, {userIosvc_, std::move(handler)});
}

//------------------------------------------------------------------------------
/** @pre `this->state() == SessionState::established`
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::publish(
    Pub pub /**< The publication to publish. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->publish(std::move(pub));
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
CPPWAMP_INLINE void Session::publish(
    Pub pub,                            /**< The publication to publish. */
    AsyncHandler<PublicationId> handler /**< Handler to invoke when
                                             the operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->publish(std::move(pub), {userIosvc_, std::move(handler)});
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
CPPWAMP_INLINE void Session::enroll(
    Procedure procedure, /**< The procedure to register. */
    CallSlot slot,       /**< The handler to execute when the RPC is invoked. */
    AsyncHandler<Registration> handler /**< Handler to invoke when
                                            the enroll operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    using std::move;
    impl_->enroll( move(procedure), move(slot), nullptr,
                   {userIosvc_, move(handler)} );
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
CPPWAMP_INLINE void Session::enroll(
    Procedure procedure, /**< The procedure to register. */
    CallSlot callSlot,   /**< The handler to execute when the RPC is invoked. */
    InterruptSlot interruptSlot, /** Handler to execute when RPC is interrupted. */
    AsyncHandler<Registration> handler /**< Handler to invoke when
                                            the enroll operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    using std::move;
    impl_->enroll( move(procedure), move(callSlot), move(interruptSlot),
                   {userIosvc_, move(handler)} );
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
CPPWAMP_INLINE void Session::unregister(
    const Registration& reg /**< The RPC registration to unregister. */
)
{
    CPPWAMP_LOGIC_CHECK(!!reg, "The registration is empty");
    if (impl_)
        impl_->unregister(reg);
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
CPPWAMP_INLINE void Session::unregister(
    const Registration& reg,   /**< The RPC registration to unregister. */
    AsyncHandler<bool> handler /**< Handler to invoke when the operation
                                    completes. */
)
{
    CPPWAMP_LOGIC_CHECK(!!reg, "The registration is empty");
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    if (impl_)
        impl_->unregister(reg, {userIosvc_, std::move(handler)});
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
CPPWAMP_INLINE RequestId Session::call(
    Rpc rpc,                     /**< Details about the RPC. */
    AsyncHandler<Result> handler /**< Handler to invoke when the
                                       operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    return impl_->call(std::move(rpc), {userIosvc_, std::move(handler)});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(Cancellation cancellation)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    return impl_->cancel(std::move(cancellation));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(AsioService& userIosvc,
                                const ConnectorList& connectors)
    : userIosvc_(userIosvc)
{
    CPPWAMP_LOGIC_CHECK(!connectors.empty(), "Connector list is empty");
    for (const auto& cnct: connectors)
        connectors_.push_back(cnct->clone());

    setWarningHandler( [](std::string warning)
    {
        std::cerr << "[CppWAMP] Warning: " << warning << "\n";
    });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::doConnect(size_t index, AsyncTask<size_t> handler)
{
    currentConnector_ = connectors_.at(index);
    std::weak_ptr<Session> self(shared_from_this());
    currentConnector_->establish(
        [this, self, index, handler](std::error_code ec,
                                     internal::ClientInterface::Ptr impl)
        {
            if (!self.expired() && !isTerminating_)
            {
                if (ec)
                {
                    if (ec == TransportErrc::aborted)
                        handler(ec);
                    else
                    {
                        auto newIndex = index + 1;
                        if (newIndex < connectors_.size())
                            doConnect(newIndex, handler);
                        else
                        {
                            state_ = State::failed;
                            if (connectors_.size() > 1)
                            {
                                ec = make_error_code(
                                         SessionErrc::allTransportsFailed);
                            }
                            std::move(handler)(ec);
                        }
                    }
                }
                else
                {
                    assert(impl);
                    state_ = State::closed;
                    impl_ = impl;
                    impl_->setLogHandlers(warningHandler_, traceHandler_);
                    impl_->setChallengeHandler(challengeHandler_);
                    std::move(handler)(index);
                }
            }
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::shared_ptr<internal::ClientInterface> Session::impl()
    {return impl_;}


} // namespace wamp
