/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CLIENT_HPP
#define CPPWAMP_CLIENT_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the asynchronous API used by a _client_ peer in WAMP
    applications. */
//------------------------------------------------------------------------------

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "asyncresult.hpp"
#include "connector.hpp"
#include "error.hpp"
#include "registration.hpp"
#include "subscription.hpp"
#include "wampdefs.hpp"
#include "internal/clientimplbase.hpp"
#include "internal/registrationimpl.hpp"
#include "internal/subscriptionimpl.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Asynchronous API used by a _client_ peer in WAMP applications.

    @par Roles
    This API supports all of the WAMP _client_ roles:
    - _Callee_
    - _Caller_
    - _Publisher_
    - _Subscriber_

    @par Asynchronous Operations
    Most of Client's member functions are asynchronous and thus require a
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
    connection via Client::disconnect. Pending post-join operations can be also
    be aborted via Client::leave. There is currently no way to abort a single
    operation without dropping the connection or leaving the realm.

    @par Terminating Asynchronous Operations
    All pending asynchronous operations can be _terminated_ by dropping the
    client connection via Client::reset or the Client destructor. By design,
    the handlers for pending operations will not be invoked if they
    were terminated in this way. This is useful if a client application needs
    to shutdown abruptly and cannot enforce the lifetime of objects accessed
    within the asynchronous operation handlers.

    @par Coroutine API
    To make it easier to chain successive asynchronous operations, a
    coroutine-based API is provided via CoroClient and CoroErrcClient.

    @see AsyncHandler, AsyncResult, CoroClient, CoroErrcClient,
         Registration, Subscription. */
//------------------------------------------------------------------------------
class Client : public std::enable_shared_from_this<Client>
{
public:
    /** Shared pointer to a Client. */
    using Ptr = std::shared_ptr<Client>;

    /** Enumerates the possible states that a Client can be in. */
    using State = SessionState;

    /** Handler type used for processing log events. */
    using LogHandler = std::function<void (std::string)>;

    /** Creates a new Client instance. */
    static Ptr create(const Connector::Ptr& connector);

    /** Creates a new Client instance. */
    static Ptr create(const ConnectorList& connectors);

    // Non-copyable
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /** Destructor. */
    virtual ~Client();

    /// @name Observers
    /// @{
    /** Returns the current state of the client session. */
    SessionState state() const;

    /** Returns the realm the client is currently joined to. */
    std::string realm() const;

    /** Returns the `Details` dictionary returned by the router as part of
        its WELCOME message. */
    Object peerInfo() const;
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
    void join(std::string realm, AsyncHandler<SessionId> handler);

    /** Asynchronously leaves the given WAMP realm. */
    void leave(AsyncHandler<std::string> handler);

    /** Asynchronously leaves the given WAMP realm with a _Reason_ URI. */
    void leave(std::string reason, AsyncHandler<std::string> handler);

    /** Disconnects the transport between the client and router. */
    void disconnect();

    /** Terminates the transport connection between the client and router. */
    void reset();
    /// @}

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    template <typename... TParams, typename TEventSlot>
    void subscribe(std::string topic,  TEventSlot&& slot,
                   AsyncHandler<Subscription> handler);

    /** Unsubscribes a subscription to a topic. */
    void unsubscribe(Subscription sub);

    /** Unsubscribes a subscription to a topic and waits for router
        acknowledgement if necessary. */
    void unsubscribe(Subscription sub, AsyncHandler<bool> handler);

    /** Publishes an argumentless event with the given topic. */
    void publish(std::string topic);

    /** Publishes an event with the given topic and argument values. */
    void publish(std::string topic, Args args);

    /** Publishes an argumentless event with the given topic and waits
        for an acknowledgement from the router. */
    void publish(std::string topic, AsyncHandler<PublicationId> handler);

    /** Publishes an event with the given topic and argument values, and waits
        for an acknowledgement from the router. */
    void publish(std::string topic, Args args,
                 AsyncHandler<PublicationId> handler);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    template <typename... Ts, typename TCallSlot>
    void enroll(std::string procedure, TCallSlot&& slot,
                AsyncHandler<Registration> handler);

    /** Unregisters a remote procedure call. */
    void unregister(Registration reg);

    /** Unregisters a remote procedure call and waits for router
        acknowledgement. */
    void unregister(Registration reg, AsyncHandler<bool> handler);

    /** Calls an argumentless remote procedure call. */
    void call(std::string procedure, AsyncHandler<Args> handler);

    /** Calls a remote procedure call with the given arguments. */
    void call(std::string procedure, Args args, AsyncHandler<Args> handler);
    /// @}

protected:
    explicit Client(const Connector::Ptr& connector);

    explicit Client(const ConnectorList& connectors);

    void doConnect(size_t index, AsyncHandler<size_t> handler);

    void doSubscribe(internal::SubscriptionBase::Ptr sub,
                     AsyncHandler<Subscription>&& handler);

    void doEnroll(internal::RegistrationBase::Ptr reg,
                  AsyncHandler<Registration>&& handler);

    std::shared_ptr<internal::ClientImplBase> impl();

    void postpone(std::function<void ()> functor);

private:
    using WeakPtr = std::weak_ptr<Client>;

    ConnectorList connectors_;
    Connector::Ptr currentConnector_;
    LogHandler warningHandler_;
    LogHandler traceHandler_;
    SessionState state_ = State::disconnected;
    bool isTerminating_ = false;
    std::shared_ptr<internal::ClientImplBase> impl_;
};

//------------------------------------------------------------------------------
/** @tparam TParams List of parameter types that the slot function expects
            after the PublicationId parameter.
    @tparam TEventSlot (Deduced) Type of the callable target to invoke when
            a matching event is received. Must meet the requirements of
            @ref EventSlot.
    @return A reference-counting Subscription handle, therafter used to manage
            the subscription's lifetime.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::subscribeError if the router replied with an `ERROR`
          response.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established`

    @details
    ### Statically-Typed Events ###
    The following form creates a _static_ event subscription that will
    invoke a @ref StaticEventSlot when a matching event is received.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    subscribe<StaticTypeList...>(topic, slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where:
    - _StaticTypeList..._ is a parameter pack of static types
    - _slot_ has the signature:
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       void (PublicationId, StaticTypeList...)
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto slot = [](PublicationId pid, std::string item, int cost)
    {
        // Handle event...
    }

    client->subscribe<std::string, int>("purchased", slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ### Dynamically-Typed Events ###
    The following form creates a _dynamic_ event subscription that will
    invoke a @ref DynamicEventSlot when a matching event is received.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    subscribe<Args>(topic, slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where:
    - Args is a bundle of dynamic arguments (positional and/or keyword)
    - _slot_ has the signature:
       ~~~~~~~~~~~~~~~~~~~~~~~~~~
       void (PublicationId, Args)
       ~~~~~~~~~~~~~~~~~~~~~~~~~~

    Example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto slot = [](PublicationId pid, Args args)
    {
        std::string name;
        int cost = 0;
        args.to(name, cost); // Throws if args are not convertible
        // Handle event...
    }

    client->subscribe<Args>("purchased", slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ### Parameterless (Void) Events ###
    The following form creates a _void_ event subscription that will
    invoke a @ref VoidEventSlot when a matching event is received.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    subscribe<void>(topic, slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where:
    - _slot_ has the signature:
       ~~~~~~~~~~~~~~~~~~~~
       void (PublicationId)
       ~~~~~~~~~~~~~~~~~~~~

    Example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto slot = [](PublicationId pid)
    {
        // Handle event...
    }

    client->subscribe<void>("purchased", slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
//------------------------------------------------------------------------------
template <typename... TParams, typename TEventSlot>
void Client::subscribe(
    std::string topic,  /**< The topic URI to subscribe to. */
    TEventSlot&& slot,  /**< Universal reference to a callable target to
                            invoke when a matching event is received. */
    AsyncHandler<Subscription> handler
       /**< Handler to invoke when the subscribe operation completes. */
)
{
    static_assert(sizeof...(TParams) > 0, "At least one template parameter is "
                                          "required for the slot's signature");
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    using std::move;
    using Sub = internal::SubscriptionImpl<TParams...>;
    using Slot = typename Sub::Slot;
    auto sub = Sub::create(impl_, move(topic), Slot(slot));
    doSubscribe(sub, move(handler));
}

//------------------------------------------------------------------------------
/** @tparam TParams List of parameter types that the slot function expects
            after the Invocation argument.
    @tparam TCallSlot (Deduced) Type of the callable target to invoke when
            a matching RPC invocation is received. Must meet the
            requirements of @ref CallSlot.
    @return A reference-counting Registration handle, therafter used to manage
            the registration's lifetime.
    @note This function was named `enroll` because `register` is a reserved
          C++ keyword.
    @pre `this->state() == SessionState::established`
    @par Error Codes
        - WampErrc::procedureAlreadyExists if the router reports that the
          procedure has already been registered for this realm.
        - WampErrc::registrationError if the router reports some
          other error.
        - Some other `std::error_code` for protocol and transport errors.
    @throws error::Logic if `this->state() != SessionState::established`

    @details
    ### Statically-Typed RPCs ###
    The following form creates a _static_ RPC registration that will
    invoke a @ref StaticCallSlot when a matching invocation is received.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    enroll<StaticTypeList...>(procedure, slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where:
    - _StaticTypeList..._ is a parameter pack of static types
    - _slot_ has the signature:
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       void (wamp::Invocation, StaticTypeList...)
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto slot = [](Invocation inv, std::string item, int cost)
    {
        // Process invocation...
        inv.yield(result); // Send result to peer
    }

    client->enroll<std::string, int>("purchase", slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ### Dynamically-Typed RPCs ###
    The following form creates a _dynamic_ RPC registration that will
    invoke a @ref DynamicCallSlot when a matching invocation is received.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    enroll<Args>(procedure, slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where:
    - Args is a bundle of dynamic arguments (positional and/or keyword)
    - _slot_ has the signature:
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       void (wamp::Invocation, Args)
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto slot = [](Invocation inv, Args args)
    {
        std::string name;
        int cost = 0;
        args.to(name, cost); // Throws if args are not convertible
        // Process invocation...
        inv.yield(result); // Send result to peer
    }

    client->enroll<Args>("purchase", slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ### Parameterless (Void) RPCs ###
    The following form creates a _void_ RPC registration that will
    invoke a @ref VoidCallSlot when a matching invocation is received.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    enroll<void>(procedure, slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    where:
    - _slot_ has the signature:
       ~~~~~~~~~~~~~~~~~~~~~~~
       void (wamp::Invocation)
       ~~~~~~~~~~~~~~~~~~~~~~~

    Example:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    auto slot = [](Invocation inv)
    {
        // Process invocation...
        inv.yield(result); // Send result to peer
    }

    client->enroll<void>("getName", slot, handler);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
//------------------------------------------------------------------------------
template <typename... Ts, typename TCallSlot>
void Client::enroll(
    std::string procedure, /**< The procedure URI to register. */
    TCallSlot&& slot,      /**< Universal reference to a callable target to
                                invoke when a matching RPC invocation is
                                received. */
    AsyncHandler<Registration> handler /**< Handler to invoke when the
                                            enroll operation completes. */
)
{
    static_assert(sizeof...(Ts) > 0, "At least one template parameter is "
                                     "required for the slot's signature");
    CPPWAMP_LOGIC_CHECK(state() == State::established,
                        "Session is not established");
    using std::move;
    using Reg = internal::RegistrationImpl<Ts...>;
    using Slot = typename Reg::Slot;
    auto reg = Reg::create(impl_, move(procedure), Slot(slot));
    doEnroll(reg, move(handler));
}

} // namespace wamp


#ifndef CPPWAMP_COMPILED_LIB
#include "internal/client.ipp"
#endif

#endif // CPPWAMP_CLIENT_HPP
