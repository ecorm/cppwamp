/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../router.hpp"
#include <atomic>
#include <cstdint>
#include <limits>
#include <thread>
#include <utility>
#include "../api.hpp"
#include "callee.hpp"
#include "caller.hpp"
#include "challenger.hpp"
#include "subscriber.hpp"
#include "peer.hpp"
#include "../bundled/sevmeyer_prng.hpp"

namespace wamp
{


namespace internal
{

//------------------------------------------------------------------------------
class RouterContext
{
public:
    RouterContext(std::shared_ptr<RouterImpl> r);
    LogLevel logLevel() const;
    void addSession(std::shared_ptr<RouterSession> s);
    void removeSession(std::shared_ptr<RouterSession> s);
    void onMessage(std::shared_ptr<RouterSession> s, WampMessage m);
    void log(LogEntry e);

private:
    std::weak_ptr<RouterImpl> router_;
};

//------------------------------------------------------------------------------
class ServerContext : public RouterContext
{
public:
    ServerContext(RouterContext r, std::shared_ptr<RouterServer> s);
    const std::string& serverName() const;
    void removeSession(std::shared_ptr<RouterSession> s);

private:
    using Base = RouterContext;
    std::weak_ptr<RouterServer> server_;
};

//------------------------------------------------------------------------------
class RouterSession : public std::enable_shared_from_this<RouterSession>
{
public:
    using Ptr = std::shared_ptr<RouterSession>;

    static Ptr create(IoStrand i, Transporting::Ptr t, AnyBufferCodec c,
                      ServerContext s)
    {
        using std::move;
        return Ptr(new RouterSession(move(i), move(t), move(c), move(s)));
    }

    SessionId id() {return id_;}

    void start(SessionId id)
    {
        id_ = id;
        logSuffix_ = ", for session " + server_.serverName() + '/' +
                     std::to_string(id);
        peer_.start();
    }

    void close() {peer_.close();}

private:
    RouterSession(IoStrand&& i, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s)
        : peer_(true, i),
          strand_(std::move(i)),
          server_(std::move(s))

    {
        peer_.setLogHandler(
            [this](LogEntry entry) {onLogEntry(std::move(entry));});
        peer_.setLogLevel(server_.logLevel());

        peer_.setInboundMessageHandler(
            [this](WampMessage msg)
            {
                server_.onMessage(shared_from_this(), std::move(msg));}
            );

        peer_.setStateChangeHandler(
            [this](SessionState s) {onStateChanged(s);} );

        peer_.open(std::move(t), std::move(c));
    }

    void onLogEntry(LogEntry&& e)
    {
        e.append(logSuffix_);
        server_.log(std::move(e));
    }

    void onStateChanged(SessionState s)
    {
        if (s == SessionState::disconnected || s == SessionState::failed)
        {
            server_.removeSession(shared_from_this());
        }
    }

    Peer peer_;
    IoStrand strand_;
    ServerContext server_;
    std::string logSuffix_;
    SessionId id_ = 0;
};

//------------------------------------------------------------------------------
class RouterServer : public Server
{
public:
    using Executor = AnyIoExecutor;

    static Ptr create(AnyIoExecutor e, ServerConfig c, RouterContext r)
    {
        return Ptr(new RouterServer(std::move(e), std::move(c), std::move(r)));
    }

    void start() override
    {
        if (!listener_)
        {
            listener_ = config_.makeListener(strand_);
            listen();
        }
    }

    void stop() override
    {
        if (listener_)
        {
            listener_->cancel();
            listener_.reset();
        }

        // TODO: Close all sessions
    }

    const std::string& name() const override {return config_.name();}

    bool isRunning() const override {return listener_ != nullptr;}

private:
    RouterServer(AnyIoExecutor e, ServerConfig&& c, RouterContext&& r)
        : strand_(boost::asio::make_strand(e)),
          config_(std::move(c)),
          router_(std::move(r))
    {}

    void listen()
    {
        if (!listener_)
            return;

        std::weak_ptr<Server> self{shared_from_this()};
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
        auto codec = config_.makeCodec(transport->info().codecId);
        auto self = std::static_pointer_cast<RouterServer>(shared_from_this());
        ServerContext ctx{router_, std::move(self)};
        auto s = RouterSession::create(strand_, std::move(transport),
                                       std::move(codec), std::move(ctx));
        sessions_.insert(s);
        router_.addSession(std::move(s));
        listen();
    }

    void removeSession(RouterSession::Ptr s) {sessions_.erase(s);}

    IoStrand strand_;
    ServerConfig config_;
    RouterContext router_;
    Listening::Ptr listener_;
    std::set<RouterSession::Ptr> sessions_;

    friend class ServerContext;
};

//------------------------------------------------------------------------------
class RouterRealmImpl
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

    RouterRealmImpl(RouterImplPtr router, FallbackExecutor fallbackExec);

    RouterImplPtr router_;
    FallbackExecutor fallbackExec_;

    friend class RouterImpl;
};

//------------------------------------------------------------------------------
class RandomIdGenerator
{
public:
    RandomIdGenerator() {}

    RandomIdGenerator(uint64_t seed) : prng_(seed) {}

    int64_t operator()()
    {
        uint64_t n = prng_();

        // Apply bit mask to constrain the distribution to consecutive integers
        // that can be represented by a double.
        static constexpr auto digits = std::numeric_limits<Real>::digits;
        static constexpr uint64_t mask = (1ull << digits) - 1u;
        n &= mask;

        // Zero is reserved according to the WAMP spec.
        if (n == 0)
            n = 1; // Neglibibly biases the 1 value by 1/2^53

        return static_cast<int64_t>(n);
    }

private:
    wamp::bundled::prng::Generator prng_;
};

//------------------------------------------------------------------------------
class RouterImpl : public std::enable_shared_from_this<RouterImpl>,
                   public Challenger, public Callee, public Caller,
                   public Subscriber

{
public:
    using Ptr = std::shared_ptr<RouterImpl>;
    using Executor = AnyIoExecutor;

    LogLevel logLevel() const {return config_.logLevel();}

private:
    RouterImpl(Executor exec, RouterConfig config)
        : strand_(boost::asio::make_strand(exec)),
          config_(std::move(config))
    {}

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    void safeAddSession(std::shared_ptr<RouterSession> s)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<RouterSession> s;
            void operator()() {self->addSession(std::move(s));}
        };

        dispatch(Dispatched{shared_from_this(), std::move(s)});
    }

    void addSession(std::shared_ptr<RouterSession> session)
    {
        SessionId id = 0;
        do
        {
            id = sessionIdGenerator_();
        }
        while (sessions_.find(id) != sessions_.end());

        sessions_.emplace(id, session);
        session->start(id);
    }

    void safeRemoveSession(std::shared_ptr<RouterSession> s)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<RouterSession> s;
            void operator()() {self->removeSession(std::move(s));}
        };

        dispatch(Dispatched{shared_from_this(), std::move(s)});
    }

    void removeSession(std::shared_ptr<RouterSession> session)
    {
        // TODO
    }

    void safeOnMessage(std::shared_ptr<RouterSession> s, WampMessage m)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<RouterSession> s;
            WampMessage m;

            void operator()()
            {
                self->onMessage(std::move(s), std::move(m));
            }
        };

        dispatch(Dispatched{shared_from_this(), std::move(s), std::move(m)});
    }

    void onMessage(std::shared_ptr<RouterSession> session, WampMessage msg)
    {
        // TODO
    }

    void log(LogEntry entry)
    {

    }

    IoStrand strand_;
    RouterConfig config_;
    std::map<std::string, Server::Ptr> services_;
    std::map<SessionId, RouterSession::Ptr> sessions_;
    RandomIdGenerator sessionIdGenerator_;

    friend class RouterContext;
};


//******************************************************************************
// RouterContext
//******************************************************************************

inline RouterContext::RouterContext(std::shared_ptr<RouterImpl> r)
    : router_(std::move(r))
{}

inline LogLevel RouterContext::logLevel() const
{
    auto r = router_.lock();
    return r ? r->logLevel() : LogLevel::off;
}

inline void RouterContext::addSession(std::shared_ptr<RouterSession> s)
{
    auto r = router_.lock();
    if (r)
        r->safeAddSession(std::move(s));
}

inline void RouterContext::removeSession(std::shared_ptr<RouterSession> s)
{
    auto r = router_.lock();
    if (r)
        r->safeRemoveSession(std::move(s));
}

inline void RouterContext::onMessage(std::shared_ptr<RouterSession> s,
                                     WampMessage m)
{
    auto r = router_.lock();
    if (r)
        r->safeOnMessage(std::move(s), std::move(m));
}

inline void RouterContext::log(LogEntry e)
{
    auto r = router_.lock();
    if (r)
        r->log(std::move(e));
}


//******************************************************************************
// ServerContext
//******************************************************************************

inline ServerContext::ServerContext(RouterContext r,
                                    std::shared_ptr<RouterServer> s)
    : Base(std::move(r)),
      server_(std::move(s))
{}

inline const std::string& ServerContext::serverName() const
{
    static const std::string expired("expired");
    auto s = server_.lock();
    if (s)
        return s->name();
    return expired;
}

inline void ServerContext::removeSession(std::shared_ptr<RouterSession> s)
{
    Base::removeSession(s);
    auto server = server_.lock();
    if (server)
        server->removeSession(std::move(s));
}


} // namespace internal


//******************************************************************************
// ServerConfig
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE ServerConfig&
ServerConfig::withAuthenticator(AuthExchangeHandler f)
{
    authenticator_ = std::move(f);
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& ServerConfig::name() const
{
    return name_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listening::Ptr
ServerConfig::makeListener(IoStrand s) const
{
    std::set<int> codecIds;
    for (const auto& c: codecBuilders_)
        codecIds.emplace(c.id());
    return listenerBuilder_(std::move(s), std::move(codecIds));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnyBufferCodec ServerConfig::makeCodec(int codecId) const
{
    for (const auto& c: codecBuilders_)
        if (c.id() == codecId)
            return c();
    assert(false);
    return {};
}


//******************************************************************************
// Router
//******************************************************************************

CPPWAMP_INLINE Router::Router(Executor exec, RouterConfig config) {}

CPPWAMP_INLINE Router::~Router() {}

CPPWAMP_INLINE Server::Ptr Router::addService(ServerConfig config) {}

CPPWAMP_INLINE void Router::removeService(const std::string& name) {}

CPPWAMP_INLINE void Router::setLogHandler(LogHandler handler) {}

CPPWAMP_INLINE void Router::setLogLevel(LogLevel level) {}

CPPWAMP_INLINE const Object& Router::roles() {}

CPPWAMP_INLINE const IoStrand& Router::strand() const {}

CPPWAMP_INLINE Server::Ptr Router::service(const std::string& name) {}

CPPWAMP_INLINE RouterRealm Router::realm(const std::string& name) {}

CPPWAMP_INLINE RouterRealm Router::realm(const std::string& name,
                                         FallbackExecutor fallbackExecutor) {}
/// @}

/// @name Operations
/// @{
void startAll();

void stopAll();
/// @}

} // namespace wamp
