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
class RequestIdChecker
{
public:
    void reset() {watermark_ = 1;}

    template <typename C>
    bool check(const C& command)
    {
        using HasRequestId = MetaBool<C::hasRequestId({})>;
        return check1(HasRequestId{}, command);
    }

private:
    template <typename C>
    bool check1(TrueType /*HasRequestId*/, const C& command)
    {
        using IsRequest = MetaBool<C::isRequest({})>;
        return check2(IsRequest{}, command);
    }

    template <typename C>
    bool check1(FalseType /*HasRequestId*/, const C&)
    {
        return true;
    }

    template <typename C>
    bool check2(TrueType /*IsRequest*/, const C& command)
    {
        if (command.requestId({}) != watermark_)
            return false;
        ++watermark_;
        return true;
    }

    bool check2(TrueType /*IsRequest*/, const Rpc& command)
    {
        if (command.requestId({}) > watermark_)
            return false;
        ++watermark_;
        return true;
    }

    template <typename C>
    bool check2(FalseType /*IsRequest*/, const C& command)
    {
        return (command.requestId({}) < watermark_);
    }

    RequestId watermark_ = 1;
};

//------------------------------------------------------------------------------
class ServerSession : public std::enable_shared_from_this<ServerSession>,
                      public RealmSession, public Challenger,
                      private PeerListener
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

private:
    using Base = RealmSession;

    ServerSession(const IoStrand& i, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s, ServerConfig::Ptr sc, Index sessionIndex)
        : Base(s.logger()),
          peer_(this, true),
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

    void onStateChanged(SessionState s, std::error_code ec) override
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
            retire();
            break;

        default:
            // Do nothing
            break;
        }
    }

    void onFailure(std::string&& why, std::error_code ec,
                   bool abortSent) override
    {
        auto action = abortSent ? AccessAction::serverAbort
                                : AccessAction::serverDisconnect;
        report({action, {}, {}, ec});
    }

    void onTrace(std::string&& messageDump) override
    {
        Base::routerLog({LogLevel::trace, std::move(messageDump)});
    }

    void onPeerHello(Realm&& realm) override
    {
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

    void onPeerAbort(Reason&& r, bool wasJoining) override
    {
        report(r.info(false));
        // ServerSession::onStateChanged will perform the retire() operation.
    }

    void onPeerAuthenticate(Authentication&& authentication) override
    {
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

    void onPeerGoodbye(Reason&& reason, bool wasShuttingDown) override
    {
        report(reason.info(false));
        if (!wasShuttingDown)
        {
            report({AccessAction::serverGoodbye,
                    errorCodeToUri(WampErrc::goodbyeAndOut)});
        }
        // peer_ already took care of sending the reply and will close the
        // session state.
    }

    void onPeerCommand(Error&& c) override            {sendToRealm(c);}
    void onPeerCommand(Pub&& c) override              {sendToRealm(c);}
    void onPeerCommand(Topic&& c) override            {sendToRealm(c);}
    void onPeerCommand(Unsubscribe&& c) override      {sendToRealm(c);}
    void onPeerCommand(Rpc&& c) override              {sendToRealm(c);}
    void onPeerCommand(CallCancellation&& c) override {sendToRealm(c);}
    void onPeerCommand(Procedure&& c) override        {sendToRealm(c);}
    void onPeerCommand(Unregister&& c) override       {sendToRealm(c);}
    void onPeerCommand(Result&& c) override           {sendToRealm(c);}

    State state() const {return peer_.state();}

    void startSession()
    {
        assert(!alreadyStarted_);
        alreadyStarted_ = true;

        std::weak_ptr<ServerSession> self = shared_from_this();

        if (routerLogLevel() == LogLevel::trace)
            enableTrace();

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
        requestIdChecker_.reset();
    }

    template <typename C>
    void sendToRealm(C& command)
    {
        if (!requestIdChecker_.check(command))
        {
            auto msg = std::string("Received ") + command.message({}).name() +
                       " message uses non-sequential request ID";
            abortSession(Reason(WampErrc::protocolViolation)
                             .withHint(std::move(msg)));
            return;
        }

        realm_.send(shared_from_this(), std::move(command));
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
    RequestIdChecker requestIdChecker_;
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
