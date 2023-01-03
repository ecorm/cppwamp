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

    void setWampSessionId(ReservedId id)
    {
        sessionInfo_.wampSessionIdHash = IdAnonymizer::anonymize(id);
    }

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
            [this, self](State st, std::error_code ec)
            {
                auto me = self.lock();
                if (me)
                    onStateChanged(st, ec);
            });

        peer_.connect(std::move(transport_), std::move(codec_));
    }

    void abort(Abort a)
    {
        shuttingDown_ = true;
        leaveRealm(false);

        auto s = state();
        bool readyToAbort = s == State::establishing ||
                            s == State::authenticating ||
                            s == State::established;
        if (readyToAbort)
        {
            auto done = peer_.abort(a);
            if (done)
                logAccess({"server-abort", a.uri(), a.options()});
            else
                logAccess({"server-abort", a.uri(), a.options(), done.error()});
        }
        else
        {
            logAccess({"server-terminate", a.uri(), a.options()});
            peer_.terminate();
        }

        clearWampSessionInfo();
    }

    void close(bool terminate, Reason r) override
    {
        if (terminate)
            this->terminate(std::move(r));
        else
            shutDown(std::move(r));
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

    State state() const {return peer_.state();}

    void shutDown(Reason reason)
    {
        if (state() != State::established)
            return terminate(std::move(reason));

        shuttingDown_ = true;
        leaveRealm(false);

        auto self = shared_from_this();
        peer_.closeSession(
            reason,
            [this, self, reason](ErrorOr<WampMessage> reply)
            {
                if (reply.has_value())
                {
                    auto& goodBye = messageCast<GoodbyeMessage>(*reply);
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

    void terminate(Reason reason)
    {
        shuttingDown_ = true;
        logAccess({"server-terminate", reason.uri(), reason.options()});
        leaveRealm();
        peer_.terminate();
    }

    void clearWampSessionInfo()
    {
        Base::clearWampId();
        realm_.reset();
        sessionInfo_.realmUri.clear();
        sessionInfo_.authId.clear();
        sessionInfo_.agent.clear();
    }

    void onStateChanged(State s, std::error_code ec)
    {
        switch (s)
        {
        case State::connecting:
            logAccess({"client-connect"});
            break;

        case State::disconnected:
            logAccess({"client-disconnect", {}, {}, ec});
            retire();
            break;

        case State::closed:
            leaveRealm();
            if (!shuttingDown_)
                peer_.establishSession();
            break;

        case State::failed:
            logAccess({"client-disconnect", {}, {}, ec});
            retire();
            break;

        default:
            // Do nothing
            break;
        }
    }

    void onMessage(WampMessage&& m)
    {
        using M = WampMsgType;
        switch (m.type())
        {
        case M::hello:        return onHello(m);
        case M::authenticate: return onAuthenticate(m);
        case M::goodbye:      return onGoodbye(m);
        case M::error:        return onError(m);
        case M::publish:      return onPublish(m);
        case M::subscribe:    return onSubscribe(m);
        case M::unsubscribe:  return onUnsubscribe(m);
        case M::call:         return onCall(m);
        case M::cancel:       return onCancelCall(m);
        case M::enroll:       return onRegister(m);
        case M::unregister:   return onUnregister(m);
        case M::yield:        return onYield(m);
        default:              assert(false && "Unexpected message type"); break;
        }
    }

    void onHello(WampMessage& msg)
    {
        auto& helloMsg = messageCast<HelloMessage>(msg);
        Realm realm{{}, std::move(helloMsg)};

        sessionInfo_.agent = realm.agent().value_or("");
        sessionInfo_.authId = realm.authId().value_or("");

        realm_ = server_.realmAt(realm.uri());
        if (!realm_)
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
        completeNow(authenticator, authExchange_);
    }

    void onAuthenticate(WampMessage& msg)
    {
        auto& authenticateMsg = messageCast<AuthenticateMessage>(msg);
        Authentication authentication{{}, std::move(authenticateMsg)};

        const auto& authenticator = serverConfig_->authenticator();
        assert(authenticator != nullptr);
        bool isExpected = authExchange_ != nullptr &&
                          state() == State::authenticating;
        if (!isExpected)
        {
            logAccess({"client-authenticate", "", {},
                       "wamp.error.protocol_violation"});
            abort(Abort(SessionErrc::protocolViolation)
                      .withHint("Unexpected AUTHENTICATE message"));
            return;
        }

        authExchange_->setAuthentication({}, std::move(authentication));
        completeNow(authenticator, authExchange_);
    }

    void onGoodbye(WampMessage& msg)
    {
        auto& goodbyeMsg = messageCast<GoodbyeMessage>(msg);
        Reason reason{{}, std::move(goodbyeMsg)};
        logAccess({"client-goodbye", reason.uri(), reason.options(),
                   "wamp.error.goodbye_and_out"});
        // peer_ already took care of sending the reply, cancelling pending
        // requests, and will close the session state.
    }

    void onError(WampMessage& m)
    {
        // TODO: Generate AccessActionInfo from message class
        auto& msg = messageCast<ErrorMessage>(m);
        Error error{{}, std::move(msg)};
        logAccess({"client-error", error.reason(), error.options()});
        realm_.yieldError(std::move(error), wampId());
    }

    void onPublish(WampMessage& m)
    {
        auto& msg = messageCast<PublishMessage>(m);
        logAccess({"client-publish", msg.topicUri(), msg.options()});
    }

    void onSubscribe(WampMessage& m)
    {
        auto& msg = messageCast<SubscribeMessage>(m);
        Topic topic{{}, std::move(msg)};
        AccessActionInfo info{"client-subscribe", topic.uri(), topic.options()};

        auto subId = realm_.subscribe(std::move(topic), shared_from_this());
        logAccess(std::move(info.withResult(subId)));
        if (!subId)
        {
            // TODO: Send ERROR message
        }
    }

    void onUnsubscribe(WampMessage& m)
    {
        auto& msg = messageCast<UnsubscribeMessage>(m);
        AccessActionInfo info{"client-unsubscribe"};
        auto done = realm_.unsubscribe(msg.subscriptionId(), wampId());
        logAccess(std::move(info.withResult(done)));
        if (!done)
        {
            // TODO: Send ERROR message
        }
    }

    void onCall(WampMessage& m)
    {
        auto& msg = messageCast<CallMessage>(m);
        Rpc rpc{{}, std::move(msg)};
        AccessActionInfo info{"client-call", rpc.procedure(), rpc.options()};
        auto done = realm_.call(std::move(rpc), wampId());
        logAccess(std::move(info.withResult(done)));
        if (!done)
        {
            // TODO: Send ERROR message
        }
    }

    void onCancelCall(WampMessage& m)
    {
        auto& msg = messageCast<CancelMessage>(m);
        realm_.cancelCall(msg.requestId(), wampId());
        logAccess({"client-cancel-call"});
    }

    void onRegister(WampMessage& m)
    {
        auto& msg = messageCast<RegisterMessage>(m);
        Procedure proc({}, std::move(msg));
        AccessActionInfo info{"client-register", proc.uri(), proc.options()};
        auto done = realm_.enroll(std::move(proc), shared_from_this());
        logAccess(std::move(info.withResult(done)));
        if (!done)
        {
            // TODO: Send ERROR message
        }
    }

    void onUnregister(WampMessage& m)
    {
        auto& msg = messageCast<UnregisterMessage>(m);
        AccessActionInfo info{"client-unregister"};
        auto done = realm_.unsubscribe(msg.registrationId(), wampId());
        logAccess(std::move(info.withResult(done)));
        if (!done)
        {
            // TODO: Send ERROR message
        }
    }

    void onYield(WampMessage& m)
    {
        auto& msg = messageCast<YieldMessage>(m);
        Result result{{}, std::move(msg)};
        logAccess({"client-yield", {}, result.options()});
        realm_.yieldResult(std::move(result), wampId());
    }

    void leaveRealm(bool clearSessionInfo = true)
    {
        realm_.leave(wampId());
        if (clearSessionInfo)
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
        if (state() == State::authenticating &&
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
        auto s = state();
        bool readyToWelcome = authExchange_ != nullptr &&
                              (s == State::establishing ||
                               s == State::authenticating);
        if (!readyToWelcome)
            return;

        const auto& realm = authExchange_->realm();
        if (!realm_)
        {
            auto errc = SessionErrc::noSuchRealm;
            logAccess({"client-hello", realm.uri(), realm.sanitizedOptions(),
                       make_error_code(errc)});
            abort({SessionErrc::noSuchRealm});
            return;
        }

        auto details = info.join({}, realm.uri(), wampId(), server_.roles());
        setAuthInfo(std::move(info));
        sessionInfo_.realmUri = realm.uri();
        logAccess({"client-hello", realm.uri(), realm.sanitizedOptions()});
        authExchange_.reset();
        peer_.welcome(wampId(), std::move(details));
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
        auto s = state();
        bool readyToReject = s == State::establishing ||
                             s == State::authenticating;
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
        completeNow(F{shared_from_this(), std::forward<Ts>(args)...});
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

    void close(bool terminate, Reason r)
    {
        std::string msg = terminate ? "Shutting down server listening on "
                                    : "Terminating server listening on ";

        msg += listener_->where() + " with reason " + r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        log({LogLevel::info, std::move(msg)});

        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (auto& s: sessions_)
            s->close(terminate, r);
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
