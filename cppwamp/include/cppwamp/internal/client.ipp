/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <cassert>
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** @post `client->state() == SessionState::disconnected`
    @return A shared pointer to the created client object. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Client::Ptr Client::create(
    const Connector::Ptr& connector /**< Connection details for the transport
                                         to use. */
)
{
    return Ptr(new Client(connector));
}

//------------------------------------------------------------------------------
/** @pre `connectors.empty() == false`
    @post `client->state() == SessionState::disconnected`
    @return A shared pointer to the created Client object.
    @throws error::Logic if `connectors.empty() == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Client::Ptr Client::create(
    const ConnectorList& connectors /**< A list of connection details for
                                         the transports to use. */
)
{
    return Ptr(new Client(connectors));
}

//------------------------------------------------------------------------------
/** @details
    Automatically invokes reset() on the client, which drops the
    connection and terminates all pending asynchronous operations. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Client::~Client()
{
    reset();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionState Client::state() const
{
    return impl_ ? impl_->state() : state_;
}

//------------------------------------------------------------------------------
/** @note If the client is not currently joined, then an empty string is
          returned. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string Client::realm() const
{
    return impl_ ? impl_->realm() : "";
}

//------------------------------------------------------------------------------
/** @note If the client is not currently joined, then an empty dictionary
          is returned. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Object Client::peerInfo() const
{
    return impl_ ? impl_->peerInfo() : Object{};
}

//------------------------------------------------------------------------------
/** @details
    Warnings occur when the client encounters problems that do not prevent
    it from proceeding normally. An example of such warnings is when a
    peer attempts to call a statically-typed RPC with invalid argument
    types.

    By default, warnings are logged to std::cerr.

    @note Changes to the warning handler start taking effect when the client
          is connecting. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::setWarningHandler(
    LogHandler handler /**< Function called to handle warnings. */
)
{
    warningHandler_ = std::move(handler);
}

//------------------------------------------------------------------------------
/** @details
    By default, debug traces are discarded.

    @note Changes to the trace handler start taking effect when the client is
          connecting. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::setTraceHandler(
    LogHandler handler /**< Function called to handle log traces. */
)
{
    traceHandler_ = std::move(handler);
}

//------------------------------------------------------------------------------
/** @details
    The client will attempt to connect using the transports that were
    specified by the Connector objects passed during create().
    If more than one transport was specified, they will be traversed in the
    same order as they appeared in the @ref ConnectorList.
    @return The index of the Connector object used to establish the connetion.
    @pre `this->state() == SessionState::disconnected`
    @post `this->state() == SessionState::connecting`
    @par Error Codes
        - TransportErrc::aborted if the connection attempt was aborted.
        - WampErrc::allTransportsFailed if more than one transport was
          specified and they all failed to connect.
        - Some other platform or transport-dependent `std::error_code` if
          only one transport was specified and it failed to connect.
    @throws error::Logic if `this->state() != SessionState::disconnected` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::connect(
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
    doConnect(0, handler);
}

//------------------------------------------------------------------------------
/** @return The Session ID of the newly established session.
    @pre `this->state() == SessionState::connected`
    @post `this->state() == SessionState::establishing`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer.
        - WampErrc::noSuchRealm if the realm does not exist.
        - WampErrc::noSuchRole if one of the client roles is not supported on
          the router.
        - WampErrc::joinError for other errors reported by the router.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::connected` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::join(
    std::string realm,              /**< The realm to join to. */
    AsyncHandler<SessionId> handler /**< Handler to invoke when the
                                         operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::closed, "Session is not closed");
    impl_->join(std::move(realm), std::move(handler));
}

//------------------------------------------------------------------------------
/** @details
    Same as leave(std::string, AsyncHandler<std::string>), except that no
    _Reason_ URI is specified. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::leave(
    AsyncHandler<std::string> handler /**< Handler to invoke when the
                                           operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->leave(std::move(handler));
}

//------------------------------------------------------------------------------
/** @return The _Reason_ URI of the `GOODBYE` response returned by the router.
    @pre `this->state() == SessionState::established`
    @post `this->state() == SessionState::shuttingDown`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer
          before a `GOODBYE` response was received.
        - Some other `std::error_code` for protocol and transport errors.
    @throw error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::leave(
    std::string reason,               ///< _Reason_ URI to send to the router.
    AsyncHandler<std::string> handler /**< Handler to invoke when the
                                           operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->leave(std::move(reason), std::move(handler));
}

//------------------------------------------------------------------------------
/** @details
    Aborts all pending asynchronous operations, invoking their handlers
    with error codes indicating that cancellation has occured.
    @post `this->state() == SessionState::disconnected` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::disconnect()
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
CPPWAMP_INLINE void Client::reset()
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
/** @details
    This function can be safely called during any client state. If the
    subscription is no longer applicable, then the unsubscribe operation
    will effectively do nothing.
    @note Duplicate unsubscribes using the same Subscription handle
          are safely ignored. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::unsubscribe(
    Subscription sub /**< The subscription to unsubscribe from. */
)
{
    if (impl_)
        impl_->unsubscribe(sub.impl_.get());
}

//------------------------------------------------------------------------------
/** @details
    If there are other local subscriptions on this client remaining for the
    same topic, then the client does not send an `UNSUBSCRIBE` message to
    the router.
    @note Duplicate unsubscribes using the same Subscription handle
          are safely ignored.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer.
        - WampErrc::noSuchSubscription if the router reports that there was
          no such subscription.
        - WampErrc::unsubscribeError if the router reports some other
          error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::unsubscribe(
    Subscription sub,          /**< The subscription to unsubscribe from. */
    AsyncHandler<bool> handler /**< Handler to invoke when the operation
                                    completes. The dummy `bool` result is `true`
                                    if successful. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    if (impl_)
        impl_->unsubscribe(sub.impl_.get(), std::move(handler));
}

//------------------------------------------------------------------------------
/** @pre `this->state() == SessionState::established`
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::publish(
    std::string topic /**< The topic URI under which to publish. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->publish(std::move(topic));
}

//------------------------------------------------------------------------------
/** @pre `this->state() == SessionState::established`
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::publish(
    std::string topic, /**< The topic URI under which to publish. */
    Args args          /**< Positional and/or keyword values to supply for
                            the event payload. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->publish(std::move(topic), std::move(args));
}

//------------------------------------------------------------------------------
/** @return The publication ID for this event.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer.
        - WampErrc::publishError if the router replies with an ERROR
          response.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::publish(
        std::string topic,  /**< The topic URI under which to publish. */
        AsyncHandler<PublicationId> handler  /**< Handler to invoke when
                                                  the operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->publish(std::move(topic), std::move(handler));
}

//------------------------------------------------------------------------------
/** @return The publication ID for this event.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer.
        - WampErrc::publishError if the router replies with an ERROR
          response.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::publish(
    std::string topic, /**< The topic URI under which to publish. */
    Args args,         /**< Positional and/or keyword values to supply for
                            the event payload. */
    AsyncHandler<PublicationId> handler /**< Handler to invoke when
                                             the operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->publish(std::move(topic), std::move(args), std::move(handler));
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any client state. If the
    registration is no longer applicable, then the unregister operation
    will effectively do nothing.
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::unregister(
    Registration reg /**< The RPC registration to unregister. */
)
{
    if (impl_)
        impl_->unregister(reg.id());
}

//------------------------------------------------------------------------------
/** @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer.
        - WampErrc::noSuchRegistration if the router reports that there is
          no such procedure registered by that name.
        - WampErrc::unregisterError if the router reports some other
          error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::unregister(
    Registration reg,          /**< The RPC registration to unregister. */
    AsyncHandler<bool> handler /**< Handler to invoke when the operation
                                    completes. The dummy `bool` result is `true`
                                    if successful. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    if (impl_)
        impl_->unregister(reg.id(), std::move(handler));
}

//------------------------------------------------------------------------------
/** @return The arguments payload yielded by the RPC
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer.
        - WampErrc::noSuchProcedure if the router reports that there is
          no such procedure registered by that name.
        - WampErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - WampErrc::callError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::call(
    std::string procedure,      /**< The procedure URI to call. */
    AsyncHandler<Args> handler  /**< Handler to invoke when the
                                     operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->call(std::move(procedure), std::move(handler));
}

//------------------------------------------------------------------------------
/** @return The arguments payload yielded by the RPC
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::sessionEnded if the operation was aborted.
        - WampErrc::sessionEndedByPeer if the session was ended by the peer.
        - WampErrc::noSuchProcedure if the router reports that there is
          no such procedure registered by that name.
        - WampErrc::invalidArgument if the callee reports that there are one
          or more invalid arguments.
        - WampErrc::callError if the router reports some other error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::call(
    std::string procedure,      /**< The procedure URI to call. */
    Args args,                  /**< Positional and/or keyword arguments
                                     to be passed to the RPC. */
    AsyncHandler<Args> handler  /**< Handler to invoke when the
                                     operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    impl_->call(std::move(procedure), std::move(args), std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Client::Client(const Connector::Ptr& connector)
    : connectors_({connector->clone()}) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Client::Client(const ConnectorList& connectors)
{
    CPPWAMP_LOGIC_CHECK(!connectors.empty(), "Connector list is empty");
    for (const auto& cnct: connectors)
        connectors_.push_back(cnct->clone());
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::doConnect(size_t index,
                                      AsyncHandler<size_t> handler)
{
    currentConnector_ = connectors_.at(index);
    WeakPtr self(shared_from_this());
    currentConnector_->establish(
        [this, self, index, handler](std::error_code ec,
                                     internal::ClientImplBase::Ptr impl)
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
                                         WampErrc::allTransportsFailed);
                            }
                            handler(ec);
                        }
                    }
                }
                else
                {
                    assert(impl);
                    state_ = State::closed;
                    impl_ = impl;
                    impl_->setLogHandlers(warningHandler_, traceHandler_);
                    handler(index);
                }
            }
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::doSubscribe(internal::SubscriptionBase::Ptr sub,
                                        AsyncHandler<Subscription>&& handler)
    {impl_->subscribe(sub, std::move(handler));}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::doEnroll(internal::RegistrationBase::Ptr reg,
                                     AsyncHandler<Registration>&& handler)
    {impl_->enroll(reg, std::move(handler));}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::shared_ptr<internal::ClientImplBase> Client::impl()
    {return impl_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Client::postpone(std::function<void ()> functor)
    {impl_->postpone(functor);}


} // namespace wamp
