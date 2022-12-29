/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
#define CPPWAMP_INTERNAL_ROUTER_SERVER_HPP

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include "../routerconfig.hpp"
#include "challenger.hpp"
#include "idgen.hpp"
#include "peer.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

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
                      public RouterSession,
                      public Challenger
{
public:
    using Ptr = std::shared_ptr<ServerSession>;
    using State = SessionState;
    using Index = uint64_t;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(const IoStrand& i, Transporting::Ptr t, AnyBufferCodec c,
                      ServerContext s, Index sessionIndex)
    {
        return Ptr(new ServerSession(i, std::move(t), std::move(c),
                                     std::move(s), sessionIndex));
    }

    ~ServerSession()
    {
        if (wampSessionId() != nullId())
        {
            server_.freeSessionId(wampSessionId());
            clearWampSessionId();
        }
    }

    Index sessionIndex() const {return sessionInfo_.serverSessionIndex;}

    State state() const {return peer_.state();}

    void start()
    {
        assert(!alreadyStarted_);
        alreadyStarted_ = true;

        std::weak_ptr<ServerSession> self = shared_from_this();

        peer_.setLogLevel(logger_->level());

        peer_.setLogHandler(
            [this, self](LogEntry entry)
            {
                auto me = self.lock();
                if (me)
                    log(std::move(entry));
            });

        peer_.setInboundMessageHandler(
            [this, self](WampMessage msg)
            {
                auto me = self.lock();
                if (me)
                    onMessage(std::move(msg));
            });

        peer_.setStateChangeHandler(
            [this, self](SessionState st, std::error_code ec)
            {
                auto me = self.lock();
                if (me)
                    onStateChanged(st, ec);
            });

        peer_.connect(std::move(transport_), std::move(codec_));
    }

    void terminate(String hint) override
    {
        shuttingDown_ = true;
        logAccess({"server-terminate", {}, {{"message", std::move(hint)}}});
        leaveRealm();
        peer_.terminate();
    }

    void shutDown(String hint, String reasonUri) override
    {
        if (state() != State::established)
            return terminate(std::move(hint));

        shuttingDown_ = true;
        leaveRealm(false);

        Reason reason{std::move(reasonUri)};
        if (!hint.empty())
            reason.withHint(std::move(hint));

        auto self = shared_from_this();
        peer_.closeSession(
            reason,
            [this, self, reason](ErrorOr<WampMessage> reply)
            {
                if (reply.has_value())
                {
                    auto& goodBye = message_cast<GoodbyeMessage>(*reply);
                    Reason peerReason({}, std::move(goodBye));
                    logAccess({"server-goodbye", reason.uri(), reason.options(),
                               peerReason.uri()});
                }
                else
                {
                    logAccess({"server-goodbye", reason.uri(), reason.options(),
                               reply.error()});
                }
                clearWampSessionInfo();
                logAccess({"server-disconnect", reason.uri(), reason.options()});
                peer_.terminate();
            });
    }

    void kick(String hint, String reasonUri)
    {
        if (state() != State::established)
            return;

        leaveRealm(false);

        Reason reason{std::move(reasonUri)};
        if (!hint.empty())
            reason.withHint(std::move(hint));

        auto self = shared_from_this();
        peer_.closeSession(
            reason,
            [this, self, reason](ErrorOr<WampMessage> reply)
            {
                if (reply.has_value())
                {
                    auto& goodBye = message_cast<GoodbyeMessage>(*reply);
                    Reason peerReason({}, std::move(goodBye));
                    logAccess({"server-goodbye", reason.uri(), reason.options(),
                               peerReason.uri()});
                }
                else
                {
                    logAccess({"server-goodbye", reason.uri(), reason.options(),
                               reply.error()});
                }

                clearWampSessionInfo();
            });
    }

    void setWampSessionId(SessionId id)
    {
        Base::setWampSessionId(id);
        sessionInfo_.wampSessionIdHash = IdAnonymizer::anonymize(id);
    }

    void sendSubscribed(RequestId, SubscriptionId) override {}

    void sendUnsubscribed(RequestId) override {}

    void sendRegistered(RequestId, RegistrationId) override {}

    void sendUnregistered(RequestId) override {}

    void sendInvocation(Invocation&&) override {}

    void sendError(Error&&) override {}

    void sendResult(Result&&) override {}

    void sendInterruption(Interruption&&) override {}

    void log(LogEntry&& e) override
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void logAccess(AccessActionInfo&& i) override
    {
        logger_->log(AccessLogEntry{sessionInfo_, std::move(i)});
    }

private:
    using Base = RouterSession;

    ServerSession(const IoStrand& i, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s, Index sessionIndex)
        : peer_(true, i, i),
          strand_(std::move(i)),
          transport_(t),
          codec_(std::move(c)),
          server_(std::move(s)),
          serverConfig_(server_.config()),
          logger_(server_.logger())
    {
        assert(serverConfig_ != nullptr);
        sessionInfo_.endpoint = t->remoteEndpointLabel();
        sessionInfo_.serverName = serverConfig_->name();
        sessionInfo_.serverSessionIndex = sessionIndex;
        logSuffix_ = " [Session " + serverConfig_->name() + '/' +
                     std::to_string(sessionInfo_.serverSessionIndex) + ']';
    }

    void clearWampSessionInfo()
    {
        if (wampSessionId() != nullId())
        {
            server_.freeSessionId(wampSessionId());
            clearWampSessionId();
        }
        sessionInfo_.realmUri.clear();
        sessionInfo_.authId.clear();
        sessionInfo_.agent.clear();
    }

    void onStateChanged(SessionState s, std::error_code ec)
    {
//        log({LogLevel::debug,
//             "ServerSession onStateChanged: " + std::to_string(int(s))});

        switch (s)
        {
        case SessionState::connecting:
            logAccess({"client-connect"});
            break;

        case SessionState::disconnected:
            logAccess({"client-disconnect", {}, {}, ec});
            retire();
            break;

        case SessionState::closed:
            leaveRealm();
            if (!shuttingDown_)
                peer_.establishSession();
            break;

        case SessionState::failed:
            logAccess({"client-disconnect", {}, {}, ec});
            retire();
            break;

        default:
            // Do nothing
            break;
        }
    }

    void onMessage(WampMessage&& msg)
    {
        log({LogLevel::debug, "ServerSession onMessage"});

        switch (msg.type())
        {
        case WampMsgType::hello:        return onHello(msg);
        case WampMsgType::authenticate: return onAuthenticate(msg);
        case WampMsgType::goodbye:      return onGoodbye(msg);

        default:
            // TODO: Authorizer
            realm_.onMessage(shared_from_this(), std::move(msg));
            break;
        }
    }

    void onHello(WampMessage& msg)
    {
        auto& helloMsg = message_cast<HelloMessage>(msg);
        Realm realm{{}, std::move(helloMsg)};

        sessionInfo_.agent = realm.agent().value_or("");
        sessionInfo_.authId = realm.authId().value_or("");

        if (!server_.realmExists(realm.uri()))
        {
            logAccess({"client-hello", realm.uri(), realm.sanitizedOptions(),
                       "wamp.error.no_such_realm", false});
            abort({SessionErrc::noSuchRealm});
            return;
        }

        const auto& authenticator = serverConfig_->authenticator();
        assert(authenticator != nullptr);
        authExchange_ = AuthExchange::create({}, std::move(realm),
                                             shared_from_this());
        dispatchVia(strand_, authenticator, authExchange_);
    }

    void onAuthenticate(WampMessage& msg)
    {
        auto& authenticateMsg = message_cast<AuthenticateMessage>(msg);
        Authentication authentication{{}, std::move(authenticateMsg)};

        const auto& authenticator = serverConfig_->authenticator();
        bool isExpected = authenticator && authExchange_ &&
                          peer_.state() == SessionState::authenticating;
        if (!isExpected)
        {
            logAccess({"client-authenticate", "", {},
                       "wamp.error.protocol_violation"});
            abort(Abort(SessionErrc::protocolViolation)
                      .withHint("Unexpected AUTHENTICATE message"));
            return;
        }

        authExchange_->setAuthentication({}, std::move(authentication));
        dispatchVia(strand_, authenticator, authExchange_);
    }

    void onGoodbye(WampMessage& msg)
    {
        auto& goodbyeMsg = message_cast<GoodbyeMessage>(msg);
        Reason reason{{}, std::move(goodbyeMsg)};
        logAccess({"client-goodbye", reason.uri(), reason.options(),
                   "wamp.error.goodbye_and_out"});
        // peer_ already took care of sending the reply, aborting pending
        // requests, and will close the session state.
    }

    void abort(Abort a)
    {
        if (state() != State::established)
            return;

        leaveRealm(false);

        auto done = peer_.abort(a);
        if (done)
            logAccess({"server-abort", a.uri(), a.options()});
        else
            logAccess({"server-abort", a.uri(), a.options(), done.error()});

        clearWampSessionInfo();
    }

    void leaveRealm(bool clearAccessLogInfo = true)
    {
        realm_.leave(shared_from_this());
        if (clearAccessLogInfo)
            clearWampSessionInfo();
    }

    void retire()
    {
        server_.removeSession(shared_from_this());
        leaveRealm();
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
        // TODO: Challenge timeout
        if (peer_.state() == SessionState::authenticating &&
            authExchange_ != nullptr)
        {
            peer_.challenge(authExchange_->challenge());
        }
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

    void welcome(AuthInfo&& info) override
    {
        auto s = peer_.state();
        bool readyToWelcome = authExchange_ != nullptr &&
                              (s == SessionState::establishing ||
                               s == SessionState::authenticating);
        if (!readyToWelcome)
            return;

        const auto& realm = authExchange_->realm();
        auto context = server_.join(realm.uri(), shared_from_this());
        if (!context)
        {
            logAccess({"client-hello", realm.uri(), realm.sanitizedOptions(),
                       context.error()});
            abort({SessionErrc::noSuchRealm});
            return;
        }

        realm_ = *context;
        auto details = info.join({}, realm.uri(), wampSessionId(),
                                 server_.roles());
        setAuthInfo(std::move(info));
        sessionInfo_.realmUri = realm.uri();
        logAccess({"client-hello", realm.uri(), realm.sanitizedOptions()});
        authExchange_.reset();
        peer_.welcome(wampSessionId(), std::move(details));
    }

    void safeWelcome(AuthInfo&& info) override
    {
        struct Dispatched
        {
            Ptr self;
            AuthInfo info;
            void operator()() {self->welcome(std::move(info));}
        };

        safelyDispatch<Dispatched>(std::move(info));
    }

    void reject(Abort&& a) override
    {
        auto s = peer_.state();
        bool readyToReject = s == SessionState::establishing ||
                             s == SessionState::authenticating;
        if (!readyToReject)
            return;

        if (authExchange_)
        {
            logAccess({"client-hello",
                       authExchange_->realm().uri(),
                       authExchange_->realm().sanitizedOptions(),
                       a.uri(),
                       false});
        }
        else
        {
            logAccess({"client-hello", {}, {}, a.uri()});
        }

        authExchange_.reset();
        clearWampSessionInfo();
        peer_.abort(std::move(a));
    }

    void safeReject(Abort&& a) override
    {
        struct Dispatched
        {
            Ptr self;
            Abort a;
            void operator()() {self->reject(std::move(a));}
        };

        safelyDispatch<Dispatched>(std::move(a));
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand_, F{shared_from_this(), std::forward<Ts>(args)...});
    }

    Peer peer_;
    IoStrand strand_;
    Transporting::Ptr transport_;
    AnyBufferCodec codec_;
    ServerContext server_;
    RealmContext realm_;
    ServerConfig::Ptr serverConfig_;
    AuthExchange::Ptr authExchange_;
    AccessSessionInfo sessionInfo_;
    RouterLogger::Ptr logger_;
    std::string logSuffix_;
    bool alreadyStarted_ = false;
    bool shuttingDown_ = false;
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
        log({LogLevel::info,
             "Starting server listening on " + listener_->where()});
        listen();
    }

    void shutDown(String hint = {}, String reasonUri = {})
    {
        if (reasonUri.empty())
            reasonUri = "wamp.close.system_shutdown";
        std::string msg =
            "Shutting down server listening on " + listener_->where() +
            " with reason " + reasonUri;
        if (!hint.empty())
            msg += ": " + hint;
        log({LogLevel::info, std::move(msg)});
        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (auto& s: sessions_)
            s->shutDown(hint, reasonUri);
    }

    void terminate(String hint = {})
    {
        std::string msg =
            "Terminating server listening on " + listener_->where();
        if (!hint.empty())
            msg += ": " + hint;
        log({LogLevel::info, std::move(msg)});
        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (auto& s: sessions_)
            s->terminate(hint);
    }

    ServerConfig::Ptr config() const {return config_;}

private:
    RouterServer(AnyIoExecutor e, ServerConfig&& c, RouterContext&& r)
        : strand_(boost::asio::make_strand(e)),
          logSuffix_(" [Server " + c.name() + ']'),
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
                    log({LogLevel::error,
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
        if (++nextSessionIndex_ == 0u)
            nextSessionIndex_ = 1u;
        auto s = ServerSession::create(strand_, std::move(transport),
                                       std::move(codec), std::move(ctx),
                                       nextSessionIndex_);
        sessions_.insert(s);
        s->start();
        listen();
    }

    void removeSession(ServerSession::Ptr s)
    {
        sessions_.erase(s);
    }

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    IoStrand strand_;
    std::set<ServerSession::Ptr> sessions_;
    std::string logSuffix_;
    RouterContext router_;
    ServerConfig::Ptr config_;
    Listening::Ptr listener_;
    RouterLogger::Ptr logger_;
    ServerSession::Index nextSessionIndex_ = 0;

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
