/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
#define CPPWAMP_INTERNAL_ROUTER_SERVER_HPP

#include <cassert>
#include <memory>
#include <utility>
#include "../routerconfig.hpp"
#include "challenger.hpp"
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
    ServerConfig::Ptr config() const;
    void removeSession(std::shared_ptr<ServerSession> s);

private:
    using Base = RouterContext;
    std::weak_ptr<RouterServer> server_;
};

//------------------------------------------------------------------------------
class ServerSession : public std::enable_shared_from_this<ServerSession>,
                      public Challenger
{
public:
    using Ptr = std::shared_ptr<ServerSession>;
    using State = SessionState;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(const IoStrand& i, Transporting::Ptr t, AnyBufferCodec c,
                      ServerContext s)
    {
        using std::move;
        return Ptr(new ServerSession(i, move(t), move(c), move(s)));
    }

    SessionId id() const {return id_;}

    const AuthorizationInfo::Ptr& authInfo() const {return authInfo_;}

    State state() const {return peer_.state();}

    void start()
    {
        peer_.establishSession();
    }

    void close()
    {
        realm_.reset();
        peer_.disconnect();
    }

    void join(RealmContext realm)
    {
        realm_ = std::move(realm);
    }

    void kick(Reason&& reason)
    {
        if (state() != State::established)
            return;

        log({LogLevel::info,
             "Kicking peer from realm with reason '" + reason.uri() + "'"});
        realm_.reset();

        auto self = shared_from_this();
        peer_.closeSession(
            reason,
            [this, self](ErrorOr<WampMessage> reply)
            {
                if (reply.has_value())
                {
                    auto& goodBye = message_cast<GoodbyeMessage>(*reply);
                    Reason peerReason({}, std::move(goodBye));
                    log({LogLevel::info,
                         "Peer left realm cleanly with reason '" +
                        peerReason.uri() + "'"});
                }
                else
                {
                    log({LogLevel::warning,
                         "Peer failed to leave realm cleanly",
                         reply.error()});
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
    ServerSession(const IoStrand& i, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s)
        : peer_(true, i, i),
          strand_(std::move(i)),
          server_(std::move(s)),
          serverConfig_(server_.config()),
          logger_(server_.logger())
    {
        id_ = server_.allocateSessionId();

        const auto& config = server_.config();
        assert(config != nullptr);
        logSuffix_ = ", for session " + config->name() + '/' +
                     IdAnonymizer::anonymize(id());

        peer_.setLogHandler(
            [this](LogEntry entry) {log(std::move(entry));});
        peer_.setLogLevel(logger_->level());

        peer_.setInboundMessageHandler(
            [this](WampMessage msg)
            {
                onMessage(std::move(msg));
            });

        peer_.setStateChangeHandler(
            [this](SessionState st) {onStateChanged(st);} );

        peer_.connect(std::move(t), std::move(c));
    }

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void onStateChanged(SessionState s)
    {
        bool transportDropped = (s == SessionState::disconnected) ||
                                (s == SessionState::failed);

        bool closedByRemotePeer = (s == SessionState::closed);

        // Don't remove session from server if it was closed gracefully, as the
        // same transport session can be reused to establish a new WAMP session.
        // See https://wamp-proto.org/wamp_latest_ietf.html#name-transport-and-session-lifet
        if (transportDropped)
            retire();
        else if (closedByRemotePeer)
            realm_.leave(shared_from_this());
    }

    void onMessage(WampMessage&& msg)
    {
        if (msg.type() == WampMsgType::hello)
        {
            auto& helloMsg = message_cast<HelloMessage>(msg);
            Realm realm{{}, std::move(helloMsg)};
            onHello(std::move(realm));
        }
        else if (msg.type() == WampMsgType::authenticate)
        {
            auto& authenticateMsg = message_cast<AuthenticateMessage>(msg);
            Authentication authentication{{}, std::move(authenticateMsg)};
            onAuthenticate(std::move(authentication));
        }
        else if (!realm_.expired())
        {
            realm_.onMessage(shared_from_this(), std::move(msg));
        }
    }

    void onHello(Realm&& realm)
    {
        const auto& authenticator = serverConfig_->authenticator();
        if (authenticator)
        {
            authExchange_ = AuthExchange::create({}, std::move(realm),
                                                 shared_from_this());
            dispatchVia(strand_, authenticator, authExchange_);
        }
        else
        {
            auto realmContext = server_.join(shared_from_this());
            if (!realmContext)
            {
                kick("wamp.error.no_such_realm",
                     "Kicked remote peer joining non-existent realm");
                return;
            }

            authInfo_ = std::make_shared<AuthorizationInfo>(realm);
            authInfo_->setSessionId(id_);
            auto details = authInfo_->welcomeDetails();
            details.emplace("roles", server_.roles());
            peer_.welcome(id_, std::move(details));
        }
    }

    void onAuthenticate(Authentication&& authentication)
    {
        const auto& authenticator = serverConfig_->authenticator();
        bool isExpected = authenticator && authExchange_ &&
                          peer_.state() == SessionState::authenticating;
        if (!isExpected)
        {
            kick("wamp.error.protocol_violation",
                 "Kicked remote peer sending unexpected AUTHENTICATE message");
        }

        authExchange_->setAuthentication({}, std::move(authentication));
        dispatchVia(strand_, authenticator, authExchange_);
    }

    void retire()
    {
        auto self = shared_from_this();
        server_.removeSession(self);
        realm_.leave(self);
    }

    void kick(std::string reasonUri, std::string logMessage,
              LogLevel severity = LogLevel::warning)
    {
        log({severity, std::move(logMessage)});
        peer_.abort({std::move(reasonUri)});
        retire();
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

    void challenge() override
    {
        if (peer_.state() != SessionState::establishing)
            return;
        assert(authExchange_ != nullptr);
        peer_.challenge(authExchange_->accessChallenge({}));
    }

    void safeChallenge() override
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->challenge();}
        };

        safelyDispatch<Dispatched>();
    }

    void welcome(Object details) override
    {
        auto s = peer_.state();
        bool readyToWelcome = s == SessionState::establishing ||
                              s == SessionState::authenticating;
        if (!readyToWelcome)
            return;
        peer_.welcome(id_, std::move(details));
    }

    void safeWelcome(Object details) override
    {
        struct Dispatched
        {
            Ptr self;
            Object details;
            void operator()() {self->welcome(std::move(details));}
        };

        safelyDispatch<Dispatched>(std::move(details));
    }

    void reject(Object details, String reasonUri) override
    {
        auto a = Abort{std::move(reasonUri)}.withOptions(std::move(details));
        rejectAuthorization(std::move(a));
    }

    void safeReject(Object details, String reasonUri) override
    {
        struct Dispatched
        {
            Ptr self;
            Abort abort;
            void operator()() {self->rejectAuthorization(std::move(abort));}
        };

        auto a = Abort{std::move(reasonUri)}.withOptions(std::move(details));
        safelyDispatch<Dispatched>(std::move(a));
    }

    void rejectAuthorization(Abort&& a)
    {
        auto s = peer_.state();
        bool readyToReject = s == SessionState::establishing ||
                             s == SessionState::authenticating;
        if (!readyToReject)
            return;
        peer_.abort(std::move(a));
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand_, F{shared_from_this(), std::forward<Ts>(args)...});
    }

    Peer peer_;
    IoStrand strand_;
    ServerContext server_;
    RealmContext realm_;
    ServerConfig::Ptr serverConfig_;
    AuthExchange::Ptr authExchange_;
    AuthorizationInfo::Ptr authInfo_;
    RouterLogger::Ptr logger_;
    std::string logSuffix_;
    SessionId id_;
};

//------------------------------------------------------------------------------
class RouterServer : public std::enable_shared_from_this<RouterServer>
{
public:
    using Ptr = std::shared_ptr<RouterServer>;
    using Executor = AnyIoExecutor;

    static Ptr create(AnyIoExecutor e, ServerConfig c, RouterContext r)
    {
        return Ptr(new RouterServer(std::move(e), std::move(c), std::move(r)));
    }

    void start()
    {
        assert(!listener_);
        listener_ = config_->makeListener(strand_);
        listen();
    }

    void shutDown()
    {
        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (auto& s: sessions_)
            s->kick(Reason{"wamp.close.system_shutdown"});
    }

    void terminate()
    {
        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (auto& s: sessions_)
            s->close();
    }

    ServerConfig::Ptr config() const {return config_;}

private:
    RouterServer(AnyIoExecutor e, ServerConfig&& c, RouterContext&& r)
        : strand_(boost::asio::make_strand(e)),
          router_(std::move(r)),
          config_(std::make_shared<ServerConfig>(std::move(c))),
          logger_(router_.logger())
    {}

    void listen()
    {
        if (!listener_)
            return;

        std::weak_ptr<RouterServer> self{shared_from_this()};
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
                    logger_->log(
                        {LogLevel::error,
                         "Failure establishing connection with remote peer",
                         transport.error()});
                }
            });
    }

    void onEstablished(Transporting::Ptr transport)
    {
        auto codec = config_->makeCodec(transport->info().codecId);
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
    std::set<ServerSession::Ptr> sessions_;
    RouterContext router_;
    ServerConfig::Ptr config_;
    Listening::Ptr listener_;
    RouterLogger::Ptr logger_;

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

inline ServerConfig::Ptr ServerContext::config() const
{
    auto s = server_.lock();
    return s ? s->config() : nullptr;

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