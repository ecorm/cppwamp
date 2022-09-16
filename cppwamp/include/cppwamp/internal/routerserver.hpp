/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
#define CPPWAMP_INTERNAL_ROUTER_SERVER_HPP

#include <memory>
#include <utility>
#include "../routerconfig.hpp"
#include "../server.hpp"
#include "idgen.hpp"
#include "peer.hpp"
#include "routercontext.hpp"

namespace wamp
{


namespace internal
{

class RouterServer;

//------------------------------------------------------------------------------
class ServerContext : public RouterContext
{
public:
    ServerContext(RouterContext r, std::shared_ptr<RouterServer> s);
    const std::string& serverName() const;
    void removeSession(std::shared_ptr<ServerSession> s);

private:
    using Base = RouterContext;
    std::weak_ptr<RouterServer> server_;
};

//------------------------------------------------------------------------------
class ServerSession : public std::enable_shared_from_this<ServerSession>
{
public:
    using Ptr = std::shared_ptr<ServerSession>;
    using State = SessionState;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(IoStrand i, Transporting::Ptr t, AnyBufferCodec c,
                      ServerContext s)
    {
        using std::move;
        return Ptr(new ServerSession(move(i), move(t), move(c), move(s)));
    }

    SessionId id() const {return authInfo_.sessionId();}

    const AuthorizationInfo& authInfo() const {return authInfo_;}

    State state() const {return peer_.state();}

    void start(SessionId id)
    {
        authInfo_.setSessionId(id);
        logSuffix_ = ", for session " + server_.serverName() + '/' +
                     IdAnonymizer::anonymize(id);
        peer_.start();
    }

    void close() {peer_.close();}

    void kick(Reason&& reason)
    {
        if (state() != State::established)
            return;

        auto self = shared_from_this();
        peer_.adjourn(
            reason,
            [this, self](ErrorOr<WampMessage> reply)
            {
                if (reply.has_value())
                {
                    auto& goodBye = message_cast<GoodbyeMessage>(*reply);
                    Reason reason({}, std::move(goodBye));
                    // TODO: Log
                }
            });
    }

    void invoke(Invocation&& inv, CompletionHandler<Outcome>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<Outcome> f;

            void operator()(ErrorOr<WampMessage> reply)
            {
                auto& me = *self;
                if (!reply)
                    me.completeNow(f, UnexpectedError(reply.error()));

                if (reply->type() == WampMsgType::yield)
                {
                    auto& msg = message_cast<YieldMessage>(*reply);
                    Result result{{}, std::move(msg)};
                    me.completeNow(f, Outcome{std::move(result)});
                }
                else if (reply->type() == WampMsgType::error)
                {
                    auto& msg = message_cast<ErrorMessage>(*reply);
                    Error error{{}, std::move(msg)};
                    me.completeNow(f, Outcome{std::move(error)});
                }
                else
                {
                    assert(false);
                }
            }
        };

        if (state() != State::established)
            return;

        auto self = shared_from_this();
        peer_.request(inv.message({}),
                      Requested{shared_from_this(), std::move(handler)});
    }

    void send(WampMessage&& msg,
              SessionState expectedState = SessionState::established)
    {
        if (state() == expectedState)
            peer_.send(msg);
    }

private:
    ServerSession(IoStrand&& i, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s)
        : peer_(true, i),
          strand_(std::move(i)),
          server_(std::move(s)),
          logger_(server_.logger())
    {
        peer_.setLogHandler(
            [this](LogEntry entry) {log(std::move(entry));});
        peer_.setLogLevel(logger_->level());

        peer_.setInboundMessageHandler(
            [this](WampMessage msg)
            {
                // TODO
                // realm_.onMessage(shared_from_this(), std::move(msg));
            });

        peer_.setStateChangeHandler(
            [this](SessionState s) {onStateChanged(s);} );

        peer_.open(std::move(t), std::move(c));
    }

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void onStateChanged(SessionState s)
    {
        if (s == SessionState::disconnected || s == SessionState::failed)
        {
            server_.removeSession(shared_from_this());
        }
    }

    template <typename F, typename... Ts>
    void complete(F&& handler, Ts&&... args)
    {
        postVia(strand_, std::move(handler), std::forward<Ts>(args)...);
    }

    template <typename F, typename... Ts>
    void completeNow(F&& handler, Ts&&... args)
    {
        dispatchVia(strand_, std::move(handler), std::forward<Ts>(args)...);
    }

    Peer peer_;
    IoStrand strand_;
    ServerContext server_;
    RealmContext realm_;
    AuthorizationInfo authInfo_;
    RouterLogger::Ptr logger_;
    std::string logSuffix_;
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

        for (auto& s: sessions_)
            s->close();
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
        auto s = ServerSession::create(strand_, std::move(transport),
                                       std::move(codec), std::move(ctx));
        sessions_.insert(s);
        listen();
    }

    void removeSession(ServerSession::Ptr s)
    {
        sessions_.erase(s);
    }

    IoStrand strand_;
    ServerConfig config_;
    RouterContext router_;
    Listening::Ptr listener_;
    std::set<ServerSession::Ptr> sessions_;

    friend class ServerContext;
};


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

inline void ServerContext::removeSession(std::shared_ptr<ServerSession> s)
{
    auto server = server_.lock();
    if (server)
        server->removeSession(std::move(s));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
