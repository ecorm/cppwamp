/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSION_HPP
#define CPPWAMP_SESSION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the asynchronous session API used by a _client_ peer in WAMP
    applications. */
//------------------------------------------------------------------------------

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "asyncresult.hpp"
#include "dialoguedata.hpp"
#include "connector.hpp"
#include "error.hpp"
#include "registration.hpp"
#include "sessiondata.hpp"
#include "subscription.hpp"
#include "wampdefs.hpp"
#include "internal/clientinterface.hpp"
#include "internal/registrationimpl.hpp"
#include "internal/subscriptionimpl.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Asynchronous session API used by a _client_ peer in WAMP applications.

    @par Roles
    This API supports all of the WAMP _client_ roles:
    - _Callee_
    - _Caller_
    - _Publisher_
    - _Subscriber_

    @par Asynchronous Operations
    Most of Session's member functions are asynchronous and thus require a
    handler function that is invoked when the operation is completed. For
    asynchronous operations that can fail, a handler taking an AsyncResult as
    a parameter is required. AsyncResult makes it impossible for handlers to
    ignore error conditions when accessing the result of an asynchronous
    operation.

    @note In the detailed documentation of asynchronous operations, items
          listed under **Returns** refer to results that are returned via
          AsyncResult.

    @par Aborting Asynchronous Operations
    All pending asynchronous operations can be _aborted_ by dropping the client
    connection via Session::disconnect. Pending post-join operations can be also
    be aborted via Session::leave. There is currently no way to abort a single
    operation without dropping the connection or leaving the realm.

    @par Terminating Asynchronous Operations
    All pending asynchronous operations can be _terminated_ by dropping the
    client connection via Session::reset or the Session destructor. By design,
    the handlers for pending operations will not be invoked if they
    were terminated in this way. This is useful if a client application needs
    to shutdown abruptly and cannot enforce the lifetime of objects accessed
    within the asynchronous operation handlers.

    @par Coroutine API
    To make it easier to chain successive asynchronous operations, a
    coroutine-based API is provided via CoroSession.

    @see AsyncHandler, AsyncResult, CoroSession, Registration, Subscription. */
//------------------------------------------------------------------------------
class Session : public std::enable_shared_from_this<Session>
{
public:
    /** Shared pointer to a Session. */
    using Ptr = std::shared_ptr<Session>;

    /** Enumerates the possible states that a Session can be in. */
    using State = SessionState;

    /** Handler type used for processing log events. */
    using LogHandler = std::function<void (std::string)>;

    /** Function type for handling pub/sub events. */
    using EventSlot = std::function<void (Event)>;

    /** Function type for handling remote procedure calls. */
    using CallSlot = std::function<void (Invocation)>;

    /** Creates a new Session instance. */
    static Ptr create(const Connector::Ptr& connector);

    /** Creates a new Session instance. */
    static Ptr create(const ConnectorList& connectors);

    /** Obtains a dictionary of roles and features supported on the client
        side. */
    static const Object& roles();

    /// @name Non-copyable
    /// @{
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    /// @}

    /** Destructor. */
    virtual ~Session();

    /// @name Observers
    /// @{
    /** Returns the current state of the session. */
    SessionState state() const;
    /// @}

    /// @name Modifiers
    /// @{
    /** Sets the log handler for warnings. */
    void setWarningHandler(LogHandler handler);

    /** Sets the log handler for debug traces. */
    void setTraceHandler(LogHandler handler);
    /// @}

    /// @name Session Management
    /// @{
    /** Asynchronously attempts to connect to a router. */
    void connect(AsyncHandler<size_t> handler);

    /** Asynchronously attempts to join the given WAMP realm. */
    void join(Realm realm, AsyncHandler<SessionInfo> handler);

    /** Asynchronously leaves the WAMP session. */
    void leave(Reason reason, AsyncHandler<Reason> handler);

    /** Disconnects the transport between the client and router. */
    void disconnect();

    /** Terminates the transport connection between the client and router. */
    void reset();
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    void subscribe(Topic topic, EventSlot slot,
                   AsyncHandler<Subscription::Ptr> handler);

    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename THead, typename... TTail, typename TEventSlot>
    void subscribe(Topic topic, TEventSlot&& slot,
                   AsyncHandler<Subscription::Ptr> handler);

    /** Unsubscribes a subscription to a topic. */
    void unsubscribe(Subscription::Ptr sub);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement, if necessary. */
    void unsubscribe(Subscription::Ptr sub, AsyncHandler<bool> handler);

    /** Publishes an event. */
    void publish(Pub pub);

    /** Publishes an event and waits for an acknowledgement from the router. */
    void publish(Pub pub, AsyncHandler<PublicationId> handler);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    void enroll(Procedure procedure, CallSlot slot,
                AsyncHandler<Registration::Ptr> handler);

    /** Registers a WAMP remote procedure call. */
    template <typename THead, typename... TTail, typename TCallSlot>
    void enroll(Procedure procedure, TCallSlot&& slot,
                AsyncHandler<Registration::Ptr> handler);

    /** Unregisters a remote procedure call. */
    void unregister(Registration::Ptr reg);

    /** Unregisters a remote procedure call and waits for router
        acknowledgement. */
    void unregister(Registration::Ptr reg, AsyncHandler<bool> handler);

    /** Calls a remote procedure. */
    void call(Rpc procedure, AsyncHandler<Result> handler);
    /// @}

protected:
    explicit Session(const Connector::Ptr& connector);

    explicit Session(const ConnectorList& connectors);

    void doConnect(size_t index, AsyncHandler<size_t> handler);

    void doSubscribe(Subscription::Ptr sub,
                     AsyncHandler<Subscription::Ptr>&& handler);

    void doEnroll(Registration::Ptr reg,
                  AsyncHandler<Registration::Ptr>&& handler);

    std::shared_ptr<internal::ClientInterface> impl();

    void postpone(std::function<void ()> functor);

private:
    ConnectorList connectors_;
    Connector::Ptr currentConnector_;
    LogHandler warningHandler_;
    LogHandler traceHandler_;
    SessionState state_ = State::disconnected;
    bool isTerminating_ = false;
    std::shared_ptr<internal::ClientInterface> impl_;
};


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
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename THead, typename... TTail, typename TEventSlot>
void Session::subscribe(
    Topic topic, /**< The topic to subscribe to. */
    TEventSlot&& slot,
        /**< Callable target to invoke when a matching event is received. */
    AsyncHandler<Subscription::Ptr> handler
        /**< Handler to invoke when the subscribe operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    using Sub = internal::StaticSubscription<THead, TTail...>;
    auto sub = Sub::create(impl_, std::move(topic),
                           std::forward<TEventSlot>(slot));
    doSubscribe(sub, std::move(handler));
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
    @throws error::Logic if `this->state() != SessionState::established` */
//------------------------------------------------------------------------------
template <typename THead, typename... TTail, typename TCallSlot>
void Session::enroll(
    Procedure procedure, /**< The procedure to register. */
    TCallSlot&& slot,    /**< Callable target to invoke when a matching RPC
                              invocation is received. */
    AsyncHandler<Registration::Ptr> handler
        /**< Handler to invoke when the enroll operation completes. */
)
{
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    using Reg = internal::StaticRegistration<THead, TTail...>;
    auto reg = Reg::create(impl_, std::move(procedure),
                           std::forward<TCallSlot>(slot));
    doEnroll(reg, std::move(handler));
}

} // namespace wamp


#ifndef CPPWAMP_COMPILED_LIB
#include "internal/session.ipp"
#endif

#endif // CPPWAMP_SESSION_HPP
