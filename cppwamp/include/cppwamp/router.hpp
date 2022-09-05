/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ROUTER_HPP
#define CPPWAMP_ROUTER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the API used by a _router_ peer in WAMP applications. */
//------------------------------------------------------------------------------

#include <functional>
#include <map>
#include <memory>
#include <set>
#include "api.hpp"
#include "anyhandler.hpp"
#include "asiodefs.hpp"
#include "chits.hpp"
#include "json.hpp"
#include "listener.hpp"
#include "peerdata.hpp"
#include "registration.hpp"
#include "subscription.hpp"
#include "internal/callee.hpp"
#include "internal/caller.hpp"
#include "internal/subscriber.hpp"
#include "internal/peer.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class Challenger
{
public:
    using Id = unsigned long long;

    virtual void challenge(Id id, Challenge challenge, Variant memento) = 0;
    virtual void welcome(Id id, Object details) = 0;
    virtual void abortJoin(Id id, Object details) = 0;
};

} // namespace internal

//------------------------------------------------------------------------------
class AuthExchange
{
public:
    const Realm& realm() const;
    const Authentication& authentication() const;
    unsigned stage() const;
    const Variant& memento() const;

    void challenge(Challenge challenge, Variant memento = {});
    void challenge(ThreadSafe, Challenge challenge, Variant memento = {});
    void welcome(Object details);
    void welcome(ThreadSafe, Object details);
    void abort(Object details = {});
    void abort(ThreadSafe, Object details = {});

private:
    std::weak_ptr<internal::Challenger> challenger_;
    Realm realm_;
    Authentication authentication_;
    Variant memento_; // Useful for keeping the authorizer stateless
    unsigned long long id_;
    unsigned stage_;
};

//------------------------------------------------------------------------------
struct RoutingServiceConfig
{
    std::string name;
    ListenerBuilder listenerBuilder;
    std::set<int> codecIds;
    AnyReusableHandler<void (AuthExchange)> authenticator;
};

namespace internal
{

class RouterImpl;

//------------------------------------------------------------------------------
class RoutingSession : public std::enable_shared_from_this<RoutingSession>
{
public:
    using Ptr = std::shared_ptr<RoutingSession>;
    using RouterPtr = std::weak_ptr<internal::RouterImpl>;

    static Ptr create(IoStrand s, RouterPtr r)
    {
        return Ptr(new RoutingSession(std::move(s), std::move(r)));
    }

    void open(Transporting::Ptr transport, AnyBufferCodec codec)
    {
        peer_.open(std::move(transport), std::move(codec));
    }

    void close()
    {
        peer_.close();
    }

private:
    RoutingSession(IoStrand s, RouterPtr r)
        : peer_(true, std::move(s)),
          router_(r)
    {
        peer_.setInboundMessageHandler(
            [this](WampMessage msg) {onInbound(std::move(msg));} );
    }

    void onInbound(WampMessage msg)
    {
        // TODO
    }

    Peer peer_;
    RouterPtr router_;
};

} // namespace internal

//------------------------------------------------------------------------------
class RoutingService : public std::enable_shared_from_this<RoutingService>
{
public:
    using Ptr = std::shared_ptr<RoutingService>;
    using Executor = AnyIoExecutor;
    using RouterPtr = std::weak_ptr<internal::RouterImpl>;

    RoutingService(AnyIoExecutor exec, RouterPtr router,
                   RoutingServiceConfig config)
        : strand_(boost::asio::make_strand(exec)),
          config_(std::move(config)),
          router_(std::move(router))
    {}

    void start()
    {
        if (!listener_)
        {
            listener_ = config_.listenerBuilder(strand_, config_.codecIds);
            listen();
        }
    }

    void stop()
    {
        if (listener_)
        {
            listener_->cancel();
            listener_.reset();
        }
    }

    const std::string& name() const {return config_.name;}

    bool isRunning() const {return listener_ != nullptr;}

private:
    void listen()
    {
        if (!listener_)
            return;

        std::weak_ptr<RoutingService> self = shared_from_this();
        listener_->establish(
            [this, self](ErrorOr<Transporting::Ptr> transport)
            {
                auto me = self.lock();
                if (!me)
                    return;

                if (transport)
                {
                    onEstablished(*transport);
                }
                else
                {
                    // TODO
                }
            });
    }

    void onEstablished(Transporting::Ptr transport)
    {
        auto codecId = transport->info().codecId;
        assert(config_.codecIds.count(codecId));
        auto s = internal::RoutingSession::create(strand_, router_);
        // TODO: Codec factory
        s->open(std::move(transport), AnyBufferCodec{json});
        listen();
    }

    IoStrand strand_;
    RoutingServiceConfig config_;
    std::weak_ptr<internal::RouterImpl> router_;
    Listening::Ptr listener_;
    std::set<internal::RoutingSession::Ptr> sessions_;
};

namespace internal
{

//------------------------------------------------------------------------------
class RouterImpl : public Challenger, public Callee, public Caller,
                   public Subscriber

{
public:
    using Ptr = std::shared_ptr<RouterImpl>;
    using Executor = AnyIoExecutor;

private:
    std::map<std::string, RoutingService::Ptr> services_;
    std::map<SessionId, RoutingSession::Ptr> sessions_;
};

} // namespace internal

//------------------------------------------------------------------------------
class RouterProxy
{
private:
    struct GenericOp { template <typename F> void operator()(F&&) {} };

public:
    /** Executor type used for I/O operations. */
    using Executor = AnyIoExecutor;

    /** Fallback executor type for user-provided handlers. */
    using FallbackExecutor = AnyCompletionExecutor;

    /** Type-erased wrapper around a WAMP event handler. */
    using EventSlot = AnyReusableHandler<void (Event)>;

    /** Type-erased wrapper around an RPC handler. */
    using CallSlot = AnyReusableHandler<Outcome (Invocation)>;

    /** Type-erased wrapper around an RPC interruption handler. */
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;

    /** Obtains the type returned by [boost::asio::async_initiate]
        (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/async_initiate.html)
        with given the completion token type `C` and signature `void(T)`.

        Token Type                   | Deduced Return Type
        ---------------------------- | -------------------
        Callback function            | `void`
        `wamp::YieldContext`         | `ErrorOr<Value>`
        `boost::asio::use_awaitable` | An awaitable yielding `ErrorOr<Value>`
        `boost::asio::use_future`    | `std::future<ErrorOr<Value>>` */
    template <typename T, typename C>
    using Deduced = decltype(
        boost::asio::async_initiate<C, void(T)>(std::declval<GenericOp&>(),
                                                std::declval<C&>()));

    /// @name Pub/Sub
    /// @{
    /** Subscribes to WAMP pub/sub events having the given topic. */
    Subscription subscribe(Topic topic, EventSlot eventSlot);

    /** Thread-safe subscribe. */
    std::future<Subscription> subscribe(ThreadSafe, Topic topic,
                                        EventSlot eventSlot);

    /** Unsubscribes a subscription to a topic. */
    bool unsubscribe(Subscription sub);

    /** Thread-safe unsubscribe. */
    std::future<bool> unsubscribe(ThreadSafe, Subscription sub);

    /** Publishes an event. */
    void publish(Pub pub);

    /** Thread-safe publish. */
    void publish(ThreadSafe, Pub pub);
    /// @}

    /// @name Remote Procedures
    /// @{
    /** Registers a WAMP remote procedure call. */
    CPPWAMP_NODISCARD ErrorOr<Registration> enroll(Procedure procedure,
                                                   CallSlot callSlot);

    /** Thread-safe enroll. */
    CPPWAMP_NODISCARD std::future<ErrorOr<Registration>>
    enroll(ThreadSafe, Procedure procedure, CallSlot callSlot);

    /** Registers a WAMP remote procedure call with an interruption handler. */
    CPPWAMP_NODISCARD ErrorOr<Registration>
    enroll(Procedure procedure, CallSlot callSlot, InterruptSlot interruptSlot);

    /** Thread-safe enroll interruptible. */
    CPPWAMP_NODISCARD std::future<ErrorOr<Registration>>
    enroll(ThreadSafe, Procedure procedure, CallSlot callSlot,
           InterruptSlot interruptSlot);

    /** Unregisters a remote procedure call. */
    void unregister(Registration reg);

    /** Thread-safe unregister. */
    void unregister(ThreadSafe, Registration reg);

    /** Calls a remote procedure. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(Rpc rpc, C&& completion);

    /** Thread-safe call. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(ThreadSafe, Rpc rpc, C&& completion);

    /** Calls a remote procedure, obtaining a token that can be used
        for cancellation. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(Rpc rpc, CallChit& chit, C&& completion);

    /** Thread-safe call with CallChit capture. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    call(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion);

    /** Calls a remote procedure with progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(Rpc rpc, C&& completion);

    /** Thread-safe call with progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(ThreadSafe, Rpc rpc, C&& completion);

    /** Calls a remote procedure with progressive results, obtaining a token
        that can be used for cancellation. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(Rpc rpc, CallChit& chit, C&& completion);

    /** Thread-safe call with CallChit capture and progressive results. */
    template <typename C>
    CPPWAMP_NODISCARD Deduced<ErrorOr<Result>, C>
    ongoingCall(ThreadSafe, Rpc rpc, CallChit& chit, C&& completion);

    /** Cancels a remote procedure using the cancel mode that was specified
        in the @ref wamp::Rpc "Rpc". */
    bool cancel(CallChit);

    /** Thread-safe cancel. */
    std::future<bool> cancel(ThreadSafe, CallChit);

    /** Cancels a remote procedure using the given mode. */
    bool cancel(CallChit, CallCancelMode mode);

    /** Thread-safe cancel with a given mode. */
    std::future<bool> cancel(ThreadSafe, CallChit, CallCancelMode mode);
    /// @}

private:
    using RouterImplPtr = std::shared_ptr<internal::RouterImpl>;

    RouterProxy(RouterImplPtr router, FallbackExecutor fallbackExec);

    RouterImplPtr router_;
    FallbackExecutor fallbackExec_;

    friend class Router;
};

//------------------------------------------------------------------------------
/** %API for a _router peer in WAMP applications. */
//------------------------------------------------------------------------------
class CPPWAMP_API Router
{
public:
    /** Executor type used for I/O operations. */
    using Executor = AnyIoExecutor;

    /** Fallback executor type for user-provided handlers. */
    using FallbackExecutor = AnyCompletionExecutor;

    /** Type-erased wrapper around a log event handler. */
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    /// @name Construction
    /// @{
    /** Constructor taking an executor. */
    explicit Router(Executor exec);

    /** Constructor taking an execution context. */
    template <typename E, EnableIf<isExecutionContext<E>()> = 0>
    explicit Router(E& context) : Router(context.get_executor()) {}

    /** Destructor. */
    ~Router();
    /// @}

    /// @name Move-only
    /// @{
    Router(const Router&) = delete;
    Router(Router&&) = default;
    Router& operator=(const Router&) = delete;
    Router& operator=(Router&&) = default;
    /// @}

    /// @name Setup
    /// @{
    RoutingService::Ptr addService(RoutingServiceConfig config);

    void removeService(const std::string& name);

    /** Sets the handler that is dispatched for logging events. */
    void setLogHandler(LogHandler handler);

    /** Sets the maximum level of log events that will be emitted. */
    void setLogLevel(LogLevel level);
    /// @}

    /// @name Observers
    /// @{
    /** Obtains a dictionary of roles and features supported by the router. */
    static const Object& roles();

    /** Obtains the execution context in which I/O operations are serialized. */
    const IoStrand& strand() const;

    RoutingService::Ptr service(const std::string& name);

    RouterProxy proxy() const;

    RouterProxy proxy(FallbackExecutor fallbackExecutor) const;
    /// @}

    /// @name Operations
    /// @{
    void startAll();

    void stopAll();
    /// @}

private:
    std::shared_ptr<internal::RouterImpl> impl_;
};

} // namespace wamp

//#ifndef CPPWAMP_COMPILED_LIB
//#include "internal/router.ipp"
//#endif

#endif // CPPWAMP_ROUTER_HPP
