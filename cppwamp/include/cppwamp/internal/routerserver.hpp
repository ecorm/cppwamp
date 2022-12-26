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
        if (wampSessionId_ != nullId())
        {
            server_.freeSessionId(wampSessionId_);
            wampSessionId_ = nullId();
        }
    }

    Index sessionIndex() const {return sessionInfo_.serverSessionIndex;}

    SessionId wampSessionId() const {return wampSessionId_;}

    const AuthorizationInfo::Ptr& authInfo() const {return authInfo_;}

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
            [this, self](SessionState st)
            {
                auto me = self.lock();
                if (me)
                    onStateChanged(st);
            });

        peer_.connect(std::move(transport_), std::move(codec_));
    }

    void terminate(String hint)
    {
        shuttingDown_ = true;
        logAccess({"server-terminate", {}, {{"message", std::move(hint)}}});
        leaveRealm();
        peer_.terminate();
    }

    void shutDown(String hint, String reasonUri)
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
        wampSessionId_ = id;
        sessionInfo_.wampSessionIdHash = IdAnonymizer::anonymize(id);
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

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void logAccess(AccessActionInfo&& i)
    {
        logger_->log(AccessLogEntry{sessionInfo_, std::move(i)});
    }

private:
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
        if (wampSessionId_ != nullId())
        {
            server_.freeSessionId(wampSessionId_);
            wampSessionId_ = nullId();
        }
        sessionInfo_.realmUri.clear();
        sessionInfo_.authId.clear();
        sessionInfo_.agent.clear();
    }

    void onStateChanged(SessionState s)
    {
        log({LogLevel::debug,
             "ServerSession onStateChanged: " + std::to_string(int(s))});

        switch (s)
        {
        case SessionState::connecting:
            logAccess({"client-connect"});
            break;

        case SessionState::disconnected:
            logAccess({"client-disconnect"});
            retire();
            break;

        case SessionState::closed:
            leaveRealm();
            if (!shuttingDown_)
                peer_.establishSession();
            break;

        case SessionState::failed:
            logAccess({"client-disconnect", {}, {},
                       make_error_code(TransportErrc::failed)});
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
            abort("wamp.error.no_such_realm");
            return;
        }

        const auto& authenticator = serverConfig_->authenticator();
        if (authenticator)
        {
            authExchange_ = AuthExchange::create({}, std::move(realm),
                                                 shared_from_this());
            dispatchVia(strand_, authenticator, authExchange_);
        }
        else
        {
            auto realmContext = server_.join(realm.uri(), shared_from_this());
            if (!realmContext)
            {
                logAccess({"client-hello", realm.uri(),
                           realm.sanitizedOptions(), realmContext.error()});
                abort("wamp.error.no_such_realm");
                return;
            }

            realm_ = *realmContext;
            authInfo_ = std::make_shared<AuthorizationInfo>(realm);
            authInfo_->setSessionId(wampSessionId_);
            sessionInfo_.realmUri = realm.uri();
            logAccess({"client-hello", realm.uri(), realm.sanitizedOptions()});
            auto details = authInfo_->welcomeDetails();
            details.emplace("roles", server_.roles());
            peer_.welcome(wampSessionId_, std::move(details));
        }
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
            abort("wamp.error.protocol_violation",
                  "Unexpected AUTHENTICATE message");
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

    void abort(String reasonUri, String hint = {})
    {
        if (state() != State::established)
            return;

        leaveRealm(false);

        auto a = Abort{std::move(reasonUri)};
        if (!hint.empty())
            a.withHint(std::move(hint));

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
        if (peer_.state() != SessionState::establishing)
            return;
        assert(authExchange_ != nullptr);
        peer_.challenge(authExchange_->challenge());
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
        peer_.welcome(wampSessionId_, std::move(details));
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
    Transporting::Ptr transport_;
    AnyBufferCodec codec_;
    ServerContext server_;
    RealmContext realm_;
    ServerConfig::Ptr serverConfig_;
    AuthExchange::Ptr authExchange_;
    AuthorizationInfo::Ptr authInfo_;
    AccessSessionInfo sessionInfo_;
    RouterLogger::Ptr logger_;
    std::string logSuffix_;
    SessionId wampSessionId_ = nullId();
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
