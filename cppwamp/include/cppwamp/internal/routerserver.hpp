/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
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
#include "peer.hpp"
#include "realmsession.hpp"
#include "routercontext.hpp"
#include "../authenticators/anonymousauthenticator.hpp"

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
    void removeSession(std::shared_ptr<ServerSession> s);

private:
    using Base = RouterContext;
    std::weak_ptr<RouterServer> server_;
};

//------------------------------------------------------------------------------
class ServerSession : public std::enable_shared_from_this<ServerSession>,
                      public RealmSession,
                      public Challenger
{
public:
    using Ptr = std::shared_ptr<ServerSession>;
    using State = SessionState;
    using Index = uint64_t;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(const IoStrand& i, Transporting::Ptr t, AnyBufferCodec c,
                      ServerContext s, ServerConfig::Ptr sc, Index sessionIndex)
    {
        return Ptr(new ServerSession(i, std::move(t), std::move(c),
                                     std::move(s), std::move(sc), sessionIndex));
    }

    void start()
    {
        auto self = shared_from_this();
        completeNow([this, self]() {doStart();});
    }

    void abort(Reason r) override
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            void operator()() {self->doAbort(std::move(r));}
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    void sendError(Error&& e, bool logOnly) override
    {
        struct Dispatched
        {
            Ptr self;
            Error e;
            bool logOnly;

            void operator()()
            {
                auto& me = *self;
                me.report(e.info(true));
                if (!logOnly)
                    me.send(std::move(e));
            }
        };

        safelyDispatch<Dispatched>(std::move(e), logOnly);
    }

    void sendSubscribed(RequestId r, SubscriptionId s) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            SubscriptionId s;

            void operator()()
            {
                auto& me = *self;
                me.report({AccessAction::serverSubscribed, r});
                me.sendMessage<SubscribedMessage>(r, s);
            }
        };

        safelyDispatch<Dispatched>(r);
    }

    void sendUnsubscribed(RequestId r, String topic) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            String t;

            void operator()()
            {
                auto& me = *self;
                me.report({AccessAction::serverUnsubscribed, r, std::move(t)});
                me.sendMessage<UnsubscribedMessage>(r);
            }
        };

        safelyDispatch<Dispatched>(r, std::move(topic));
    }

    void sendPublished(RequestId r, PublicationId p) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            PublicationId p;

            void operator()()
            {
                auto& me = *self;
                me.report({AccessAction::serverPublished, r});
                me.sendMessage<PublishedMessage>(r, p);
            }
        };

        safelyDispatch<Dispatched>(r, p);
    }

    void sendEvent(Event&& ev, String topic) override
    {
        struct Dispatched
        {
            Ptr self;
            Event e;
            String t;

            void operator()()
            {
                auto& me = *self;
                me.report(e.info(std::move(t)));
                me.send(std::move(e));
            }
        };

        safelyDispatch<Dispatched>(std::move(ev), std::move(topic));
    }

    void sendRegistered(RequestId reqId, RegistrationId regId) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId reqId;
            RegistrationId regId;

            void operator()()
            {
                auto& me = *self;
                me.report({AccessAction::serverRegistered, reqId});
                me.sendMessage<RegisteredMessage>(reqId, regId);
            }
        };

        safelyDispatch<Dispatched>(reqId, regId);
    }

    void sendUnregistered(RequestId r, String procedure) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            String p;

            void operator()()
            {
                auto& me = *self;
                me.report({AccessAction::serverUnregistered, r, std::move(p)});
                me.sendMessage<UnregisteredMessage>(r);
            }
        };

        safelyDispatch<Dispatched>(r, std::move(procedure));
    }

    void onSendInvocation(Invocation&& inv) override
    {
        struct Dispatched
        {
            Ptr self;
            Invocation i;

            void operator()()
            {
                auto& me = *self;
                me.report(i.info());
                me.send(std::move(i));
            }
        };

        safelyDispatch<Dispatched>(std::move(inv));
    }

    void sendResult(Result&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            Result r;

            void operator()()
            {
                auto& me = *self;
                me.report(r.info(true));
                me.send(std::move(r));
            }
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    void sendInterruption(Interruption&& intr) override
    {
        struct Dispatched
        {
            Ptr self;
            Interruption i;

            void operator()()
            {
                auto& me = *self;
                me.report(i.info());
                self->send(std::move(i));
            }
        };

        safelyDispatch<Dispatched>(std::move(intr));
    }

    void log(LogEntry e) override
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void report(AccessActionInfo i) override
    {
        logger_->log(AccessLogEntry{sessionInfo_, std::move(i)});
    }

private:
    using Base = RealmSession;

    ServerSession(const IoStrand& i, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s, ServerConfig::Ptr sc, Index sessionIndex)
        : peer_(true),
          strand_(std::move(i)),
          transport_(t),
          codec_(std::move(c)),
          server_(std::move(s)),
          serverConfig_(std::move(sc)),
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

    void doStart()
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

    void doAbort(Reason r)
    {
        shuttingDown_ = true;
        report({AccessAction::serverAbort, {}, r.options(), r.uri()});
        leaveRealm();
        peer_.abort(r);
    }

    template <typename T>
    void send(T&& messageData)
    {
        if (state() != State::established)
            return;
        peer_.send(messageData.message({}));
    }

    template <typename TMessage, typename... TArgs>
    void sendMessage(TArgs&&... args)
    {
        if (state() != State::established)
            return;
        TMessage msg{std::forward<TArgs>(args)...};
        peer_.send(msg);
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
            report({AccessAction::clientConnect});
            break;

        case State::disconnected:
            report({AccessAction::clientDisconnect, {}, {}, ec});
            retire();
            break;

        case State::closed:
            leaveRealm();
            if (!shuttingDown_)
                peer_.establishSession();
            break;

        case State::failed:
            report({AccessAction::clientDisconnect, {}, {}, ec});
            retire();
            break;

        default:
            // Do nothing
            break;
        }
    }

    void onMessage(WampMessage&& m)
    {
        if (!checkSequentialRequestId(m))
            return;

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

        auto roles = realm.roles();
        if (roles)
            setFeatures(*roles);

        sessionInfo_.agent = realm.agent().value_or("");
        sessionInfo_.authId = realm.authId().value_or("");

        realm_ = server_.realmAt(realm.uri());
        if (!realm_)
        {
            auto errc = WampErrc::noSuchRealm;
            report(realm.info().withError(errc));
            doAbort({errc});
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
            auto errc = WampErrc::protocolViolation;
            report(authentication.info().withError(errc));
            doAbort(Reason(errc).withHint("Unexpected AUTHENTICATE message"));
            return;
        }

        report(authentication.info());
        authExchange_->setAuthentication({}, std::move(authentication));
        completeNow(authenticator, authExchange_);
    }

    void onGoodbye(WampMessage& msg)
    {
        auto& goodbyeMsg = messageCast<GoodbyeMessage>(msg);
        Reason reason{{}, std::move(goodbyeMsg)};
        report(reason.info(false));
        report({AccessAction::serverGoodbye,
                errorCodeToUri(WampErrc::goodbyeAndOut)});
        // peer_ already took care of sending the reply, cancelling pending
        // requests, and will close the session state.
    }

    void onError(WampMessage& m)
    {
        auto& msg = messageCast<ErrorMessage>(m);
        Error error{{}, std::move(msg)};
        report(error.info(false));
        realm_.yieldError(shared_from_this(), std::move(error));
    }

    void onPublish(WampMessage& m)
    {
        auto& msg = messageCast<PublishMessage>(m);
        Pub pub({}, std::move(msg));
        report(pub.info());
        realm_.publish(shared_from_this(), std::move(pub));
    }

    void onSubscribe(WampMessage& m)
    {
        auto& msg = messageCast<SubscribeMessage>(m);
        Topic topic{{}, std::move(msg)};
        report(topic.info());
        realm_.subscribe(shared_from_this(), std::move(topic));
    }

    void onUnsubscribe(WampMessage& m)
    {
        auto& msg = messageCast<UnsubscribeMessage>(m);
        auto reqId = msg.requestId();
        report({AccessAction::clientUnsubscribe, reqId});
        realm_.unsubscribe(shared_from_this(), msg.subscriptionId(),
                           msg.requestId());
    }

    void onCall(WampMessage& m)
    {
        auto& msg = messageCast<CallMessage>(m);
        Rpc rpc{{}, std::move(msg)};
        report(rpc.info());
        realm_.call(shared_from_this(), std::move(rpc));
    }

    void onCancelCall(WampMessage& m)
    {
        auto& msg = messageCast<CancelMessage>(m);
        CallCancellation cncl({}, std::move(msg));
        report(cncl.info());
        realm_.cancelCall(shared_from_this(), std::move(cncl));
    }

    void onRegister(WampMessage& m)
    {
        auto& msg = messageCast<RegisterMessage>(m);
        Procedure proc({}, std::move(msg));
        report(proc.info());
        realm_.enroll(shared_from_this(), std::move(proc));
    }

    void onUnregister(WampMessage& m)
    {
        auto& msg = messageCast<UnregisterMessage>(m);
        auto reqId = msg.requestId();
        report({AccessAction::clientUnregister, reqId});
        realm_.unsubscribe(shared_from_this(), msg.registrationId(),
                           msg.requestId());
    }

    void onYield(WampMessage& m)
    {
        auto& msg = messageCast<YieldMessage>(m);
        Result result{{}, std::move(msg)};
        report(result.info(false));
        realm_.yieldResult(shared_from_this(), std::move(result));
    }

    void leaveRealm()
    {
        realm_.leave(wampId());
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
        postAny(strand_, std::move(handler), std::forward<Ts>(args)...);
    }

    template <typename F, typename... Ts>
    void completeNow(F&& handler, Ts&&... args)
    {
        dispatchAny(strand_, std::move(handler), std::forward<Ts>(args)...);
    }

    bool checkSequentialRequestId(const WampMessage& m)
    {
        if (!m.hasRequestId())
            return true;

        // Allow progressive calls to reference past request IDs
        bool isCall = m.type() == WampMsgType::call;
        bool needsSequential = !isCall && m.isRequest();
        auto requestId = m.requestId();

        if (needsSequential)
        {
            if (requestId != expectedRequestId_)
            {
                auto msg = std::string("Received ") + m.name() +
                           " message uses non-sequential request ID";
                doAbort(Reason(WampErrc::protocolViolation)
                            .withHint(std::move(msg)));
                return false;
            }
            ++expectedRequestId_;
        }
        else
        {
            auto limit = expectedRequestId_ + (isCall ? 1 : 0);
            if (requestId >= limit)
            {
                auto msg = std::string("Received ") + m.name() +
                           " message uses future request ID";
                doAbort(Reason(WampErrc::protocolViolation)
                            .withHint(std::move(msg)));
                return false;
            }
            if (isCall && (requestId == expectedRequestId_))
                ++expectedRequestId_;
        }

        return true;
    }

    void challenge() override
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->doChallenge();}
        };

        safelyDispatch<Dispatched>();
    }

    void doChallenge()
    {
        // TODO: Challenge timeout
        if (state() == State::authenticating &&
            authExchange_ != nullptr)
        {
            auto c = authExchange_->challenge();
            report(c.info());
            peer_.challenge(std::move(c));
        }
    }

    void welcome(AuthInfo&& info) override
    {
        struct Dispatched
        {
            Ptr self;
            AuthInfo info;
            void operator()() {self->doWelcome(std::move(info));}
        };

        safelyDispatch<Dispatched>(std::move(info));
    }

    void doWelcome(AuthInfo&& info)
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
            auto errc = WampErrc::noSuchRealm;
            doAbort({errc});
            return;
        }

        realm_.join(shared_from_this());
        auto details = info.join({}, realm.uri(), wampId(), server_.roles());
        setAuthInfo(std::move(info));
        sessionInfo_.realmUri = realm.uri();
        sessionInfo_.wampSessionId = wampId();
        authExchange_.reset();
        report({AccessAction::serverWelcome, realm.uri(), realm.options()});
        peer_.welcome(wampId(), std::move(details));
    }

    void reject(Reason&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            void operator()() {self->doReject(std::move(r));}
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    void doReject(Reason&& r)
    {
        auto s = state();
        bool readyToReject = s == State::establishing ||
                             s == State::authenticating;
        if (!readyToReject)
            return;

        authExchange_.reset();
        clearWampSessionInfo();
        peer_.abort(std::move(r));
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
    RequestId expectedRequestId_ = 1;
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
        auto self = shared_from_this();
        boost::asio::dispatch(strand_, [this, self](){doStart();});
    }

    void close(Reason r)
    {
        struct Posted
        {
            Ptr self;
            Reason r;
            void operator()() {self->doClose(std::move(r));}
        };

        boost::asio::dispatch(strand_, Posted{shared_from_this(),
                                              std::move(r)});
    }

    ServerConfig::Ptr config() const {return config_;}

private:
    RouterServer(AnyIoExecutor e, ServerConfig&& c, RouterContext&& r)
        : executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          logSuffix_(" [Server " + c.name() + ']'),
          router_(std::move(r)),
          config_(std::make_shared<ServerConfig>(std::move(c))),
          logger_(router_.logger())
    {
        if (!config_->authenticator())
            config_->withAuthenticator(AnonymousAuthenticator{});
    }

    void doStart()
    {
        assert(!listener_);
        listener_ = config_->makeListener(strand_);
        inform("Starting server listening on " + listener_->where());
        listen();
    }

    void doClose(Reason r)
    {
        std::string msg = "Shutting down server listening on " +
                          listener_->where() + " with reason " + r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        inform(std::move(msg));

        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (auto& s: sessions_)
            s->abort(r);
    }

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
                    alert("Failure establishing connection with remote peer",
                          transport.error());
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
        auto s = ServerSession::create(boost::asio::make_strand(executor_),
                                       std::move(transport),
                                       std::move(codec), std::move(ctx),
                                       config_, nextSessionIndex_);
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

    void inform(String msg) {log({LogLevel::info, std::move(msg)});}

    void warn(String msg, std::error_code ec = {})
    {
        log({LogLevel::warning, std::move(msg), ec});
    }

    void alert(String msg, std::error_code ec = {})
    {
        log({LogLevel::error, std::move(msg), ec});
    }

    AnyIoExecutor executor_;
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

inline void ServerContext::removeSession(std::shared_ptr<ServerSession> s)
{
    auto server = server_.lock();
    if (server)
        server->removeSession(std::move(s));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
