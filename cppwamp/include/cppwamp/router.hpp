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

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include "api.hpp"
#include "anyhandler.hpp"
#include "asiodefs.hpp"
#include "chits.hpp"
#include "codec.hpp"
#include "listener.hpp"
#include "logging.hpp"
#include "peerdata.hpp"
#include "registration.hpp"
#include "subscription.hpp"
#include "tagtypes.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------
class RouterSession;
class RouterServer;
class RouterRealmImpl;
class RouterImpl;

} // namespace internal


//------------------------------------------------------------------------------
class ServerConfig
{
public:
    using AuthExchangeHandler = AnyReusableHandler<void (AuthExchange)>;

    template <typename S>
    explicit ServerConfig(std::string name, S&& transportSettings)
        : name_(std::move(name)),
          listenerBuilder_(std::forward<S>(transportSettings))
    {}

    template <typename... TFormats>
    ServerConfig& withFormats(TFormats... formats)
    {
        codecBuilders_ = {BufferCodecBuilder{formats}...};
        return *this;
    }

    ServerConfig& withAuthenticator(AuthExchangeHandler f);

    const std::string& name() const;

private:
    Listening::Ptr makeListener(IoStrand s) const;

    AnyBufferCodec makeCodec(int codecId) const;

    std::string name_;
    ListenerBuilder listenerBuilder_;
    std::vector<BufferCodecBuilder> codecBuilders_;
    AnyReusableHandler<void (AuthExchange)> authenticator_;

    friend class internal::RouterServer;
};

//------------------------------------------------------------------------------
class Server : public std::enable_shared_from_this<Server>
{
public:
    using Ptr = std::shared_ptr<Server>;

    virtual ~Server() {}

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual const std::string& name() const = 0;

    virtual bool isRunning() const = 0;
};

//------------------------------------------------------------------------------
class RouterRealm
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
    using ImplPtr = std::shared_ptr<internal::RouterRealmImpl>;

    RouterRealm(ImplPtr impl, FallbackExecutor fallbackExec);

    ImplPtr realm_;
    FallbackExecutor fallbackExec_;

    friend class Router;
};

//------------------------------------------------------------------------------
class RouterConfig
{
public:
    RouterConfig& withLogLevel(LogLevel l) {logLevel_ = l; return *this;}

    LogLevel logLevel() const {return logLevel_;}

private:
    LogLevel logLevel_ = LogLevel::warning;
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
    explicit Router(Executor exec, RouterConfig config = {});

    /** Constructor taking an execution context. */
    template <typename E, EnableIf<isExecutionContext<E>()> = 0>
    explicit Router(E& context, RouterConfig config = {})
        : Router(context.get_executor(), std::move(config))
    {}

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
    Server::Ptr addService(ServerConfig config);

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

    Server::Ptr service(const std::string& name);

    RouterRealm realm(const std::string& name);

    RouterRealm realm(const std::string& name,
                      FallbackExecutor fallbackExecutor);
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

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/router.ipp"
#endif

#endif // CPPWAMP_ROUTER_HPP
