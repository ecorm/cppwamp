/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../session.hpp"
#include <cassert>
#include "../api.hpp"
#include "client.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    const AnyIoExecutor& exec /**< Executor from which Session will extract
                                   a strand for its internal I/O operations */
)
{
    return Ptr(new Session(exec, exec));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    const AnyIoExecutor& exec, /**< Executor from which Session will extract
                                    a strand for its internal I/O operations */
    AnyIoExecutor userExec     /**< Fallback executor to use for user
                                    handlers that have none bound. */
)
{
    return Ptr(new Session(exec, std::move(userExec)));
}

//------------------------------------------------------------------------------
/** @details
    The provided executor serves as a fallback when asynchronous operation
    handlers don't bind a specific executor (in lieu of using the system
    executor as fallback.
    @post `session->state() == SessionState::disconnected`
    @return A shared pointer to the created session object. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    AnyIoExecutor userExec,   /**< Fallback executor with which to execute
                                   user-provided handlers. */
    LegacyConnector connector /**< Connection details for the transport to use. */
)
{
    return Ptr(new Session(std::move(userExec),
                           ConnectorList{std::move(connector)}));
}

//------------------------------------------------------------------------------
/** @details
    The provided executor serves as a fallback when asynchronous operation
    handlers don't bind a specific executor (in lieu of using the system
    executor as fallback.
    @pre `connectors.empty() == false`
    @post `session->state() == SessionState::disconnected`
    @return A shared pointer to the created Session object.
    @throws error::Logic if `connectors.empty() == true` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Ptr Session::create(
    AnyIoExecutor userExec,  /**< Fallback executor with which to execute
                                  user-provided handlers. */
    ConnectorList connectors /**< A list of connection details for
                                  the transports to use. */
)
{
    CPPWAMP_LOGIC_CHECK(!connectors.empty(), "Connector list is empty");
    return Ptr(new Session(std::move(userExec), std::move(connectors)));
}

//------------------------------------------------------------------------------
/** @details
    The dictionary is structured as per `HELLO.Details.roles`, as desribed in
    the ["Client: Role and Feature Announcement"][feature-announcement]
    section of the advanced WAMP specification.

    [feature-announcement]: https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.7.1.1.1 */
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
    // TODO: Make destructor non-virtual once CoroSession is removed
    stateChangeHandler_ = {};
    reset();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnyIoExecutor Session::userExecutor() const
{
    return userExecutor_;
}

//------------------------------------------------------------------------------
/** @deprecated Use wamp::Session::userExecutor instead. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE AnyIoExecutor Session::userIosvc() const
{
    return userExecutor();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE IoStrand Session::strand() const
{
    return strand_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SessionState Session::state() const
{
    auto impl = std::weak_ptr<internal::ClientInterface>(impl_).lock();
    return impl ? impl->state() : state_.load();
}

//------------------------------------------------------------------------------
/** @details
    Warnings occur when the session encounters problems that do not prevent
    it from proceeding normally. An example of such warnings is when a
    peer attempts to send an event with arguments that does not match the types
    of a statically-typed event slot.

    By default, warnings are discarded. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setWarningHandler(
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    if (impl_)
        impl_->setWarningHandler(handler);
    warningHandler_ = std::move(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setWarningHandler(LogHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setWarningHandler(
    ThreadSafe,
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    struct Dispatched
    {
        Ptr self;
        LogHandler handler;
        void operator()() {self->setWarningHandler(std::move(handler));}
    };

    dispatchViaStrand(Dispatched{shared_from_this(), std::move(handler)});
}

//------------------------------------------------------------------------------
/** @details
    By default, debug traces are discarded. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setTraceHandler(
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    if (impl_)
        impl_->setTraceHandler(handler);
    traceHandler_ = std::move(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setTraceHandler(LogHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setTraceHandler(
    ThreadSafe,
    LogHandler handler /**< Callable handler of type `<void (std::string)>`. */
)
{
    struct Dispatched
    {
        Ptr self;
        LogHandler handler;
        void operator()() {self->setTraceHandler(std::move(handler));}
    };

    dispatchViaStrand(Dispatched{shared_from_this(), std::move(handler)});
}

//------------------------------------------------------------------------------
/** @note No state change events are fired when the session object is
          destructing. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setStateChangeHandler(
    StateChangeHandler handler /**< Callable handler of type `<void (SessionState)>`. */
)
{
    if (impl_)
        impl_->setStateChangeHandler(handler);
    stateChangeHandler_ = std::move(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setStateChangeHandler(StateChangeHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setStateChangeHandler(
    ThreadSafe,
    StateChangeHandler handler /**< Callable handler of type `<void (SessionState)>`. */
)
{
    struct Dispatched
    {
        Ptr self;
        StateChangeHandler handler;
        void operator()() {self->setStateChangeHandler(std::move(handler));}
    };

    dispatchViaStrand(Dispatched{shared_from_this(), std::move(handler)});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setChallengeHandler(
    ChallengeHandler handler /**< Callable handler of type `<void (Challenge)>`. */
)
{
    if (impl_)
        impl_->setChallengeHandler(handler);
    challengeHandler_ = std::move(handler);
}

//------------------------------------------------------------------------------
/** @copydetails Session::setChallengeHandler(ChallengeHandler) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setChallengeHandler(
    ThreadSafe,
    ChallengeHandler handler /**< Callable handler of type `<void (Challenge)>`. */
    )
{
    struct Dispatched
    {
        Ptr self;
        ChallengeHandler handler;
        void operator()() {self->setChallengeHandler(std::move(handler));}
    };

    dispatchViaStrand(Dispatched{shared_from_this(), std::move(handler)});
}

//------------------------------------------------------------------------------
/** @details
    If `this->state() != SessionState::authenticating`, then the authentication
    is discarded and not sent.  */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::authenticate(
    Authentication auth /**< Contains the authentication signature
                             and other options. */
)
{
    if (state() != State::authenticating)
    {
        warn("Session::authenticate called while not "
             "in the authenticating state");
        return;
    }

    impl_->authenticate(std::move(auth));
}

//------------------------------------------------------------------------------
/** @copydetails Session::authenticate(Authentication) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::authenticate(
    ThreadSafe,
    Authentication auth /**< Contains the authentication signature
                             and other options. */
)
{
    struct Dispatched
    {
        Ptr self;
        Authentication auth;
        void operator()() {self->authenticate(std::move(auth));}
    };

    dispatchViaStrand(Dispatched{shared_from_this(), std::move(auth)});
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
    if (isConnecting)
    {
        setState(State::disconnected);
        currentConnector_->cancel();
    }
    else if (impl_)
    {
        // Peer will fire the disconnected state change event.
        state_ = State::disconnected;
        impl_->disconnect();
        impl_.reset();
    }
    else
    {
        setState(State::disconnected);
    }
}

//------------------------------------------------------------------------------
/** @copydetails Session::disconnect */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::disconnect(ThreadSafe)
{
    auto self = shared_from_this();
    boost::asio::dispatch(strand(), [self]() {self->disconnect();});
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
    setState(State::disconnected);
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
/** @copydetails Session::reset */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::reset(ThreadSafe)
{
    auto self = shared_from_this();
    boost::asio::dispatch(strand(), [self]() {self->reset();});
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any session state. If the
    subscription is no longer applicable, then the unsubscribe operation
    will effectively do nothing.
    @see Subscription, ScopedSubscription
    @note Duplicate unsubscribes using the same Subscription object
          are safely ignored.
    @pre `bool(sub) == true`
    @throws error::Logic if the given subscription is empty */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unsubscribe(
    Subscription sub /**< The subscription to unsubscribe from. */
)
{
    CPPWAMP_LOGIC_CHECK(bool(sub), "The subscription is empty");
    if (impl_)
        impl_->unsubscribe(sub);
}

//------------------------------------------------------------------------------
/** @copydetails Session::unsubscribe(Subscription) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unsubscribe(
    ThreadSafe,
    Subscription sub /**< The subscription to unsubscribe from. */
    )
{
    CPPWAMP_LOGIC_CHECK(bool(sub), "The subscription is empty");
    auto self = shared_from_this();
    boost::asio::dispatch(strand(), [self, sub]() {self->unsubscribe(sub);});
}

//------------------------------------------------------------------------------
/** @details
    If `this->state() != SessionState::established`, then the publication is
    discarded and not sent. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::publish(
    Pub pub /**< The publication to publish. */
)
{
    if (state() != State::established)
    {
        warn("Session::publish called while not in the established state");
        return;
    }

    impl_->publish(std::move(pub));
}

//------------------------------------------------------------------------------
/** @copydetails Session::publish(Pub pub) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::publish(
    ThreadSafe,
    Pub pub /**< The publication to publish. */
)
{
    struct Dispatched
    {
        Ptr self;
        Pub pub;
        void operator()() {self->publish(std::move(pub));}
    };

    dispatchViaStrand(Dispatched{shared_from_this(), std::move(pub)});
}

//------------------------------------------------------------------------------
/** @details
    This function can be safely called during any session state. If the
    registration is no longer applicable, then the unregister operation
    will effectively do nothing.
    @see Registration, ScopedRegistration
    @note Duplicate unregistrations using the same Registration handle
          are safely ignored.
    @pre `bool(reg) == true`
    @throws error::Logic if the given registration is empty */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unregister(
    Registration reg /**< The RPC registration to unregister. */
)
{
    CPPWAMP_LOGIC_CHECK(bool(reg), "The registration is empty");
    if (impl_)
        impl_->unregister(reg);
}

//------------------------------------------------------------------------------
/** @copydetails Session::unregister(Registration) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::unregister(
    ThreadSafe,
    Registration reg /**< The RPC registration to unregister. */
    )
{
    CPPWAMP_LOGIC_CHECK(bool(reg), "The registration is empty");
    auto self = shared_from_this();
    boost::asio::dispatch(strand(), [self, reg]() {self->unregister(reg);});
}

//------------------------------------------------------------------------------
/** @details
    If `this->state() != SessionState::established`, then the cancellation
    is discarded and not sent. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    CallChit chit /**< Contains the request ID of the call to cancel. */
    )
{
    return cancel(chit, chit.cancelMode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    ThreadSafe,
    CallChit chit /**< Contains the request ID of the call to cancel. */
    )
{
    return cancel(threadSafe, chit, chit.cancelMode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    CallChit chit,      /**< Contains the request ID of the call to cancel. */
    CallCancelMode mode /**< The mode with which to cancel the call. */
    )
{
    if (state() != State::established)
    {
        warn("Session::cancel called while not in the established state");
        return;
    }
    return impl_->cancelCall(chit.requestId(), mode);
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    ThreadSafe,
    CallChit chit,      /**< Contains the request ID of the call to cancel. */
    CallCancelMode mode /**< The mode with which to cancel the call. */
    )
{
    auto self = shared_from_this();
    boost::asio::dispatch(strand(),
                          [self, chit, mode]() {self->cancel(chit, mode);});
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    CallCancellation cancellation /**< Contains the request ID
                                       and other options. */
)
{
    if (state() != State::established)
    {
        warn("Session::cancel called while not in the established state");
        return;
    }
    return impl_->cancelCall(cancellation.requestId(), cancellation.mode());
}

//------------------------------------------------------------------------------
/** @copydetails Session::cancel(CallChit, CallCancelMode) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::cancel(
    ThreadSafe,
    CallCancellation cancellation  /**< Contains the request ID
                                         and other options. */
)
{
    auto self = shared_from_this();
    boost::asio::dispatch(strand(),
                          [self, cancellation]() {self->cancel(cancellation);});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::asyncConnect(ConnectionWishList wishes,
                                          CompletionHandler<size_t>&& handler)
{
    assert(!wishes.empty());
    if (!checkState<size_t>(State::disconnected, handler))
        return;

    setState(State::connecting);
    isTerminating_ = false;
    currentConnector_ = nullptr;

    // This makes it easier to transport the move-only completion handler
    // through the gauntlet of intermediary handler functions.
    auto sharedHandler =
        std::make_shared<CompletionHandler<size_t>>(std::move(handler));

    doConnect(std::move(wishes), 0, std::move(sharedHandler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(const AnyIoExecutor& exec,
                                AnyIoExecutor userExec)
    : strand_(boost::asio::make_strand(exec)),
      userExecutor_(std::move(userExec)),
      state_(State::disconnected)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Session::Session(AnyIoExecutor userExec,
                                ConnectorList connectors)
    : strand_(boost::asio::make_strand(connectors.at(0).executor())),
      userExecutor_(std::move(userExec)),
      legacyConnectors_(std::move(connectors)),
      state_(State::disconnected)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::warn(std::string log)
{
    if (warningHandler_)
        dispatchVia(userExecutor_, warningHandler_, std::move(log));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::setState(SessionState s)
{
    auto old = state_.exchange(s);
    if (old != s && stateChangeHandler_)
        postVia(userExecutor_, stateChangeHandler_, s);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::doConnect(
    ConnectionWishList&& wishes, size_t index, std::shared_ptr<CompletionHandler<size_t>> handler)
{
    using std::move;
    struct Established
    {
        std::weak_ptr<Session> self;
        ConnectionWishList wishes;
        size_t index;
        std::shared_ptr<CompletionHandler<size_t>> handler;

        void operator()(ErrorOr<Transporting::Ptr> transport)
        {
            auto locked = self.lock();
            if (!locked)
                return;

            auto& me = *locked;
            if (me.isTerminating_)
                return;

            if (!transport)
            {
                me.onConnectFailure(move(wishes), index, transport.error(),
                                    move(handler));
            }
            else if (me.state() == State::connecting)
            {
                auto codec = wishes.at(index).makeCodec();
                me.onConnectSuccess(index, move(codec), move(*transport),
                                    move(handler));
            }
            else
            {
                auto ec = make_error_code(TransportErrc::aborted);
                postVia(me.userExecutor_, std::move(*handler),
                        UnexpectedError(ec));
            }
        }
    };

    currentConnector_ = wishes.at(index).makeConnector(strand_);
    currentConnector_->establish(
        Established{shared_from_this(), move(wishes), index, move(handler)});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::onConnectFailure(
    ConnectionWishList&& wishes, size_t index, std::error_code ec,
    std::shared_ptr<CompletionHandler<size_t>> handler)
{
    if (ec == TransportErrc::aborted)
    {
        postVia(userExecutor_, std::move(*handler), UnexpectedError(ec));
    }
    else
    {
        auto newIndex = index + 1;
        if (newIndex < wishes.size())
        {
            doConnect(std::move(wishes), newIndex, std::move(handler));
        }
        else
        {
            setState(State::failed);
            if (wishes.size() > 1)
                ec = make_error_code(SessionErrc::allTransportsFailed);
            postVia(userExecutor_, std::move(*handler), UnexpectedError(ec));
        }
    }
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Session::onConnectSuccess(
    size_t index, AnyBufferCodec&& codec, Transporting::Ptr transport,
    std::shared_ptr<CompletionHandler<size_t>> handler)
{
    impl_ = internal::Client::create(std::move(codec),
                                     std::move(transport));
    impl_->initialize(userExecutor_, warningHandler_, traceHandler_,
                      stateChangeHandler_, challengeHandler_);
    setState(State::closed);
    postVia(userExecutor_, std::move(*handler), index);
}

} // namespace wamp
