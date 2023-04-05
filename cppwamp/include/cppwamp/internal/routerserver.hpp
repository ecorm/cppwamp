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
#include "../authenticators/anonymousauthenticator.hpp"
#include "challenger.hpp"
#include "commandinfo.hpp"
#include "peer.hpp"
#include "realmsession.hpp"
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
        completeNow([this, self]() {startSession();});
    }

    void onRouterAbort(Reason&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            void operator()() {self->abortSession(std::move(r));}
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    void onRouterCommand(Error&& e) override        {sendRouterCommand(std::move(e));}
    void onRouterCommand(Subscribed&& s) override   {sendRouterCommand(std::move(s));}
    void onRouterCommand(Unsubscribed&& u) override {sendRouterCommand(std::move(u));}
    void onRouterCommand(Published&& p) override    {sendRouterCommand(std::move(p));}
    void onRouterCommand(Event&& e) override        {sendRouterCommand(std::move(e));}
    void onRouterCommand(Registered&& r) override   {sendRouterCommand(std::move(r));}
    void onRouterCommand(Unregistered&& u) override {sendRouterCommand(std::move(u));}
    void onRouterCommand(Invocation&& i) override   {sendRouterCommand(std::move(i));}
    void onRouterCommand(Result&& r) override       {sendRouterCommand(std::move(r));}
    void onRouterCommand(Interruption&& i) override {sendRouterCommand(std::move(i));}

private:
    using Base = RealmSession;

    ServerSession(const IoStrand& i, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s, ServerConfig::Ptr sc, Index sessionIndex)
        : Base(s.logger()),
          peer_(true),
          strand_(std::move(i)),
          transport_(t),
          codec_(std::move(c)),
          server_(std::move(s)),
          serverConfig_(std::move(sc))
    {
        assert(serverConfig_ != nullptr);
        Base::setTransportInfo({t->remoteEndpointLabel(), serverConfig_->name(),
                                sessionIndex});
    }

    State state() const {return peer_.state();}

    void startSession()
    {
        assert(!alreadyStarted_);
        alreadyStarted_ = true;

        std::weak_ptr<ServerSession> self = shared_from_this();

        peer_.setLogLevel(Base::routerLogLevel());

        peer_.listenLogged(
            [this, self](LogEntry entry)
            {
                auto me = self.lock();
                if (me)
                    Base::routerLog(std::move(entry));
            });

        peer_.setInboundMessageHandler(
            [this, self](Message msg)
            {
                auto me = self.lock();
                if (me)
                    onMessage(std::move(msg));
            });

        peer_.listenStateChanged(
            [this, self](State st, std::error_code ec)
            {
                auto me = self.lock();
                if (me)
                    onStateChanged(st, ec);
            });

        peer_.connect(std::move(transport_), std::move(codec_));
    }

    void abortSession(Reason r)
    {
        shuttingDown_ = true;
        report({AccessAction::serverAbort, {}, r.options(), r.uri()});
        leaveRealm();
        peer_.abort(r);
    }

    template <typename C>
    void sendRouterCommand(C&& command)
    {
        struct Dispatched
        {
            Ptr self;
            C command;

            void operator()()
            {
                auto& me = *self;
                if (me.state() != State::established)
                    return;
                me.peer_.send(std::move(command));
            }
        };

        completeNow(Dispatched{shared_from_this(), std::forward<C>(command)});
    }

    void resetWampSessionInfo()
    {
        Base::resetSessionInfo();
        realm_.reset();
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

    void onMessage(Message&& m)
    {
        if (!checkSequentialRequestId(m))
            return;

        auto self = shared_from_this();

        using K = MessageKind;
        switch (m.kind())
        {
        case K::hello:        return onHello(m);
        case K::authenticate: return onAuthenticate(m);
        case K::goodbye:      return onGoodbye(m);
        case K::error:        return sendToRealm<Error>(m);
        case K::publish:      return sendToRealm<Pub>(m);
        case K::subscribe:    return sendToRealm<Topic>(m);
        case K::unsubscribe:  return sendToRealm<Unsubscribe>(m);
        case K::call:         return sendToRealm<Rpc>(m);
        case K::cancel:       return sendToRealm<CallCancellation>(m);
        case K::enroll:       return sendToRealm<Procedure>(m);
        case K::unregister:   return sendToRealm<Unregister>(m);
        case K::yield:        return sendToRealm<Result>(m);
        default:              assert(false && "Unexpected message type"); break;
        }
    }

    void onHello(Message& msg)
    {
        Realm realm{{}, std::move(msg)};
        Base::report(realm.info());
        Base::setHelloInfo(realm);

        realm_ = server_.realmAt(realm.uri());
        if (realm_.expired())
        {
            auto errc = WampErrc::noSuchRealm;
            abortSession({errc});
            return;
        }

        const auto& authenticator = serverConfig_->authenticator();
        assert(authenticator != nullptr);
        authExchange_ = AuthExchange::create({}, std::move(realm),
                                             shared_from_this());
        completeNow(authenticator, authExchange_);
    }

    void onAuthenticate(Message& msg)
    {
        Authentication authentication{{}, std::move(msg)};
        Base::report(authentication.info());

        const auto& authenticator = serverConfig_->authenticator();
        assert(authenticator != nullptr);

        bool isExpected = authExchange_ != nullptr &&
                          state() == State::authenticating;
        if (!isExpected)
        {
            auto errc = WampErrc::protocolViolation;
            abortSession(Reason(errc).withHint("Unexpected AUTHENTICATE message"));
            return;
        }

        authExchange_->setAuthentication({}, std::move(authentication));
        completeNow(authenticator, authExchange_);
    }

    void onGoodbye(Message& msg)
    {
        Reason reason{{}, std::move(msg)};
        report(reason.info(false));
        report({AccessAction::serverGoodbye,
                errorCodeToUri(WampErrc::goodbyeAndOut)});
        // peer_ already took care of sending the reply, cancelling pending
        // requests, and will close the session state.
    }

    template <typename TCommand>
    void sendToRealm(Message& msg)
    {
        realm_.send(shared_from_this(), TCommand{{}, std::move(msg)});
    }

    void leaveRealm()
    {
        realm_.leave(wampId());
        resetWampSessionInfo();
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

    bool checkSequentialRequestId(const Message& m)
    {
        if (!m.hasRequestId())
            return true;

        // Allow progressive calls to reference past request IDs
        bool isCall = m.kind() == MessageKind::call;
        bool needsSequential = !isCall && m.isRequest();
        auto requestId = m.requestId();

        if (needsSequential)
        {
            if (requestId != expectedRequestId_)
            {
                auto msg = std::string("Received ") + m.name() +
                           " message uses non-sequential request ID";
                abortSession(Reason(WampErrc::protocolViolation)
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
                abortSession(Reason(WampErrc::protocolViolation)
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
            void operator()() {self->onChallenge();}
        };

        safelyDispatch<Dispatched>();
    }

    void onChallenge()
    {
        // TODO: Challenge timeout
        if (state() == State::authenticating &&
            authExchange_ != nullptr)
        {
            auto c = authExchange_->challenge();
            report(c.info());
            peer_.send(std::move(c));
        }
    }

    void welcome(AuthInfo&& info) override
    {
        struct Dispatched
        {
            Ptr self;
            AuthInfo info;
            void operator()() {self->onWelcome(std::move(info));}
        };

        safelyDispatch<Dispatched>(std::move(info));
    }

    void onWelcome(AuthInfo&& info)
    {
        auto s = state();
        bool readyToWelcome = authExchange_ != nullptr &&
                              (s == State::establishing ||
                               s == State::authenticating);
        if (!readyToWelcome)
            return;

        const auto& realm = authExchange_->realm();
        if (!realm_.join(shared_from_this()))
        {
            abortSession({WampErrc::noSuchRealm});
            return;
        }

        auto details = info.join({}, realm.uri(), wampId(),
                                 RouterFeatures::providedRoles());
        Base::setWelcomeInfo(std::move(info));
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
            void operator()() {self->onReject(std::move(r));}
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    void onReject(Reason&& r)
    {
        auto s = state();
        bool readyToReject = s == State::establishing ||
                             s == State::authenticating;
        if (!readyToReject)
            return;

        authExchange_.reset();
        resetWampSessionInfo();
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
        boost::asio::dispatch(strand_, [this, self](){startListening();});
    }

    void close(Reason r)
    {
        struct Posted
        {
            Ptr self;
            Reason r;
            void operator()() {self->onClose(std::move(r));}
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

    void startListening()
    {
        assert(!listener_);
        listener_ = config_->makeListener(strand_);
        inform("Starting server listening on " + listener_->where());
        listen();
    }

    void onClose(Reason r)
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
