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
#include <unordered_map>
#include <utility>
#include "../routeroptions.hpp"
#include "../authenticators/anonymousauthenticator.hpp"
#include "challenger.hpp"
#include "commandinfo.hpp"
#include "cppwamp/internal/networkpeer.hpp"
#include "networkpeer.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"
#include "timeoutscheduler.hpp"

namespace wamp
{

namespace internal
{

class RouterServer;
class ServerSession;

using ServerSessionKey = uint64_t;

//------------------------------------------------------------------------------
class ServerContext : public RouterContext
{
public:
    ServerContext(RouterContext r, const std::shared_ptr<RouterServer>& s);
    void removeSession(ServerSessionKey key);
    void armChallengeTimeout(ServerSessionKey key);
    void cancelChallengeTimeout(ServerSessionKey key);

private:
    using Base = RouterContext;
    std::weak_ptr<RouterServer> server_;
};

//------------------------------------------------------------------------------
class RequestIdChecker
{
public:
    void reset() {inboundWatermark_ = 1;}

    template <typename C>
    bool checkInbound(const C& command)
    {
        using K = MessageKind;
        auto r = requestId(command);

        switch (C::messageKind({}))
        {
        case K::error: case K::result:
            return r <= outboundWatermark_;

        case K::publish: case K::subscribe: case K::unsubscribe:
        case K::enroll: case K::unregister:
            if (r == inboundWatermark_)
            {
                ++inboundWatermark_;
                return true;
            }
            return false;

        case K::cancel:
            return r < inboundWatermark_;

        case K::call:
            if (r < inboundWatermark_)
            {
                return true;
            }
            else if (r == inboundWatermark_)
            {
                ++inboundWatermark_;
                return true;
            }
            return false;

        default: break;
        }

        return true;
    }

    void onOutbound(const Message& msg)
    {
        auto r = msg.requestId();
        if (r > outboundWatermark_)
            outboundWatermark_ = r;
    }

private:
    template <typename C>
    RequestId requestId(const C& command)
    {
        using HasRequestId = MetaBool<C::hasRequestId({})>;
        return getRequestId(HasRequestId{}, command);
    }

    template <typename C>
    RequestId getRequestId(TrueType, const C& command)
    {
        return command.requestId({});
    }

    template <typename C>
    RequestId getRequestId(FalseType, const C&)
    {
        return 0;
    }

    RequestId inboundWatermark_ = 1;
    RequestId outboundWatermark_ = 1;
};

//------------------------------------------------------------------------------
class ServerSession : public std::enable_shared_from_this<ServerSession>,
                      public RouterSession, public Challenger,
                      private PeerListener
{
public:
    using Ptr = std::shared_ptr<ServerSession>;
    using State = SessionState;
    using Key = ServerSessionKey;

    ServerSession(AnyIoExecutor e, Transporting::Ptr&& t, AnyBufferCodec&& c,
                  ServerContext&& s, ServerOptions::Ptr sc, Key sessionKey)
        : Base(s.logger()),
          executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          peer_(std::make_shared<NetworkPeer>(true)),
          transport_(t),
          codec_(std::move(c)),
          server_(std::move(s)),
          ServerOptions_(std::move(sc)),
          uriValidator_(server_.uriValidator()),
        key_(sessionKey)
    {
        assert(ServerOptions_ != nullptr);
        auto info = t->connectionInfo();
        info.setServer({}, ServerOptions_->name(), sessionKey);
        Base::connect(std::move(info));
        peer_->listen(this);
    }

    ~ServerSession() override = default;

    Key index() const {return key_;}

    void start()
    {
        auto self = shared_from_this();
        dispatch([this, self]() {startSession();});
    }

    void onChallengeTimeout()
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->reject(Reason{WampErrc::timeout});}
        };

        safelyDispatch<Dispatched>();
    }

    ServerSession(const ServerSession&) = delete;
    ServerSession(ServerSession&&) = delete;
    ServerSession& operator=(const ServerSession&) = delete;
    ServerSession& operator=(ServerSession&&) = delete;

private:
    using Base = RouterSession;

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

    void onRouterMessage(Message&& msg) override
    {
        struct Dispatched
        {
            Ptr self;
            Message m;

            void operator()()
            {
                auto& me = *self;
                if (me.state() != State::established)
                    return;
                me.requestIdChecker_.onOutbound(m);
                me.peer_->sendMessage(m);
            }
        };

        dispatch(Dispatched{shared_from_this(), std::move(msg)});
    }

    void onPeerDisconnect() override
    {
        report({AccessAction::clientDisconnect});
        retire();
    }

    void onPeerFailure(std::error_code ec, bool abortSent,
                       std::string why) override
    {
        auto action = abortSent ? AccessAction::serverAbort
                                : AccessAction::serverDisconnect;
        Object opts;
        if (!why.empty())
            opts.emplace("message", std::move(why));
        report({action, {}, std::move(opts), ec});
        retire();
    }

    void onPeerTrace(std::string&& messageDump) override
    {
        Base::routerLog({LogLevel::trace, std::move(messageDump)});
    }

    void onPeerHello(Petition&& hello) override
    {
        Base::report(hello.info());

        realm_ = server_.realmAt(hello.uri());
        if (realm_.expired())
        {
            auto errc = WampErrc::noSuchRealm;
            abortSession({errc});
            return;
        }

        authExchange_ = AuthExchange::create({}, std::move(hello),
                                             shared_from_this());
        ServerOptions_->authenticator()->authenticate(authExchange_, executor_);
    }

    void onPeerAbort(Reason&& r, bool) override
    {
        report(r.info(false));
        // ServerSession::onStateChanged will perform the retire() operation.
    }

    void onPeerChallenge(Challenge&&) override {assert(false);}

    void onPeerAuthenticate(Authentication&& authentication) override
    {
        Base::report(authentication.info());

        const bool isExpected = authExchange_ != nullptr &&
                                state() == State::authenticating;
        if (!isExpected)
        {
            return abortSession(Reason(WampErrc::protocolViolation).
                                withHint("Unexpected AUTHENTICATE message"));
        }

        server_.cancelChallengeTimeout(key_);
        authExchange_->setAuthentication({}, std::move(authentication));
        ServerOptions_->authenticator()->authenticate(authExchange_, executor_);
    }

    void onPeerGoodbye(Reason&& reason, bool wasShuttingDown) override
    {
        report(reason.info(false));

        if (!uriValidator_->checkError(reason.uri()))
            return abortSession(Reason(WampErrc::invalidUri));

        if (!wasShuttingDown)
        {
            report({AccessAction::serverGoodbye,
                    errorCodeToUri(WampErrc::goodbyeAndOut)});
            peer_->close();
        }

        leaveRealm();
        if (!wasShuttingDown)
            peer_->establishSession();
    }

    void onPeerMessage(Message&& m) override
    {
        using K = MessageKind;
        switch (m.kind())
        {
        case K::error:          return sendToRealm(Error{PassKey{},     std::move(m)});
        case K::publish:        return sendToRealm(Pub{{},              std::move(m)});
        case K::subscribe:      return sendToRealm(Topic{{},            std::move(m)});
        case K::unsubscribe:    return sendToRealm(Unsubscribe{{},      std::move(m)});
        case K::call:           return sendToRealm(Rpc{{},              std::move(m)});
        case K::cancel:         return sendToRealm(CallCancellation{{}, std::move(m)});
        case K::enroll:         return sendToRealm(Procedure{{},        std::move(m)});
        case K::unregister:     return sendToRealm(Unregister{{},       std::move(m)});
        case K::yield:          return sendToRealm(Result{{},           std::move(m)});
        default: assert(false && "Unexpected MessageKind enumerator");
        }
    }

    State state() const {return peer_->state();}

    void startSession()
    {
        assert(!alreadyStarted_);
        alreadyStarted_ = true;

        const std::weak_ptr<ServerSession> self = shared_from_this();

        if (routerLogLevel() == LogLevel::trace)
            enableTracing();

        peer_->connect(std::move(transport_), std::move(codec_));
        peer_->establishSession();
        report({AccessAction::clientConnect});
    }

    void abortSession(Reason r)
    {
        report({AccessAction::serverAbort, {}, r.options(), r.uri()});
        peer_->abort(r);
        leaveRealm();
    }

    void close()
    {
        Base::close();
        realm_.reset();
        authExchange_.reset();
        requestIdChecker_.reset();
    }

    template <typename C>
    void sendToRealm(C&& command)
    {
        if (!requestIdChecker_.checkInbound(command))
        {
            auto msg = std::string("Received ") + command.message({}).name() +
                       " message uses non-sequential request ID";
            abortSession(Reason(WampErrc::protocolViolation)
                             .withHint(std::move(msg)));
            return;
        }

        realm_.send(shared_from_this(), std::forward<C>(command));
    }

    void leaveRealm()
    {
        realm_.leave(shared_from_this());
        close();
    }

    void retire()
    {
        leaveRealm();

        // Removing session from server must be done after all cleanup
        // operations to avoid reference count prematurely reaching zero.
        auto self = shared_from_this();
        post([self]() {self->server_.removeSession(self->key_);});
    }

    void challenge()
    {
        if ((state() == State::authenticating) && (authExchange_ != nullptr))
        {
            auto c = authExchange_->challenge();
            report(c.info());
            peer_->send(std::move(c));
            server_.armChallengeTimeout(key_);
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

    void welcome(SessionInfoImpl::Ptr info)
    {
        const auto s = state();
        const bool readyToWelcome = authExchange_ != nullptr &&
                                    (s == State::establishing ||
                                     s == State::authenticating);
        if (!readyToWelcome)
        {
            authExchange_.reset();
            return;
        }

        auto& hello = authExchange_->hello({});
        auto realmUri = std::move(hello.uri({}));
        auto welcomeDetails = info->join(realmUri,
                                         RouterFeatures::providedRoles());
        info->setAgent(hello.agentOrEmptyString({}), hello.features());
        authExchange_.reset();
        Base::join(std::move(info));

        if (!realm_.join(shared_from_this()))
            return abortSession({WampErrc::noSuchRealm});

        report({AccessAction::serverWelcome, std::move(realmUri),
                welcomeDetails});
        peer_->welcome(wampId(), std::move(welcomeDetails));
    }

    void safeWelcome(SessionInfoImpl::Ptr info) override
    {
        struct Dispatched
        {
            Ptr self;
            SessionInfoImpl::Ptr info;
            void operator()() {self->welcome(std::move(info));}
        };

        safelyDispatch<Dispatched>(std::move(info));
    }

    void reject(Reason&& r)
    {
        authExchange_.reset();
        const auto s = state();
        const bool readyToReject = s == State::establishing ||
                                   s == State::authenticating;
        if (!readyToReject)
            return;

        close();
        report({AccessAction::serverAbort, {}, r.options(), r.errorCode()});
        peer_->abort(std::move(r));
    }

    void safeReject(Reason&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            void operator()() {self->reject(std::move(r));}
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(strand_, std::forward<F>(handler), std::forward<Ts>(args)...);
    }

    template <typename F, typename... Ts>
    void dispatch(F&& handler, Ts&&... args)
    {
        dispatchAny(strand_, std::forward<F>(handler),
                    std::forward<Ts>(args)...);
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        dispatch(F{shared_from_this(), std::forward<Ts>(args)...});
    }

    AnyIoExecutor executor_;
    IoStrand strand_;
    std::shared_ptr<NetworkPeer> peer_;
    Transporting::Ptr transport_;
    AnyBufferCodec codec_;
    ServerContext server_;
    RealmContext realm_;
    ServerOptions::Ptr ServerOptions_;
    AuthExchange::Ptr authExchange_;
    RequestIdChecker requestIdChecker_;
    UriValidator::Ptr uriValidator_;
    Key key_;
    bool alreadyStarted_ = false;
};

//------------------------------------------------------------------------------
class RouterServer : public std::enable_shared_from_this<RouterServer>
{
public:
    using Ptr = std::shared_ptr<RouterServer>;
    using Executor = AnyIoExecutor;

    static Ptr create(Executor e, ServerOptions c, RouterContext r)
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

    ServerOptions::Ptr config() const {return options_;}

private:
    using SessionTimeoutScheduler = TimeoutScheduler<ServerSession::Key>;

    RouterServer(AnyIoExecutor e, ServerOptions&& c, RouterContext&& r)
        : executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          challengeTimeouts_(SessionTimeoutScheduler::create(strand_)),
          logSuffix_(" [Server " + c.name() + ']'),
          router_(std::move(r)),
        options_(std::make_shared<ServerOptions>(std::move(c))),
          logger_(router_.logger())
    {
        if (!options_->authenticator())
            options_->withAuthenticator(AnonymousAuthenticator::create());
    }

    void startListening()
    {
        assert(!listener_);
        listener_ = options_->makeListener(strand_);
        inform("Starting server listening on " + listener_->where());

        const std::weak_ptr<RouterServer> self{shared_from_this()};
        challengeTimeouts_->listen(
            [self](ServerSession::Key n)
            {
                auto me = self.lock();
                if (me)
                    me->onChallengeTimeout(n);
            });

        listen();
    }

    void onClose(Reason r)
    {
        std::string msg = "Shutting down server listening on " +
                          listener_->where() + " with reason " + r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        inform(std::move(msg));

        challengeTimeouts_->unlisten();

        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (const auto& kv: sessions_)
            kv.second->abort(r);
    }

    void listen()
    {
        if (!listener_)
            return;

        const std::weak_ptr<RouterServer> self{shared_from_this()};
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
        auto codec = options_->makeCodec(transport->info().codecId);
        auto self = std::static_pointer_cast<RouterServer>(shared_from_this());
        ServerContext ctx{router_, self};
        if (++nextSessionIndex_ == 0u)
            nextSessionIndex_ = 1u;
        auto index = nextSessionIndex_;
        auto s = std::make_shared<ServerSession>(
            executor_, std::move(transport), std::move(codec), std::move(ctx),
            options_, index);
        sessions_.emplace(index, s);
        s->start();
        listen();
    }

    void onChallengeTimeout(ServerSession::Key key)
    {
        auto found = sessions_.find(key);
        if (found == sessions_.end())
            return;
        found->second->onChallengeTimeout();
    }

    void removeSession(ServerSession::Key key)
    {
        struct Dispatched
        {
            Ptr self;
            ServerSessionKey key;
            void operator()() const {self->sessions_.erase(key);}
        };

        safelyDispatch<Dispatched>(key);
    }

    void armChallengeTimeout(ServerSessionKey key)
    {
        struct Dispatched
        {
            Ptr self;
            ServerSessionKey key;

            void operator()() const
            {
                auto me = *self;
                auto timeout = me.options_->challengeTimeout();
                if (timeout != unspecifiedTimeout)
                    me.challengeTimeouts_->insert(key, timeout);
            }
        };

        safelyDispatch<Dispatched>(key);
    }

    void cancelChallengeTimeout(ServerSessionKey key)
    {
        struct Dispatched
        {
            Ptr self;
            ServerSessionKey key;
            void operator()() const {self->challengeTimeouts_->erase(key);}
        };

        safelyDispatch<Dispatched>(key);
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

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand_, F{shared_from_this(), std::forward<Ts>(args)...});
    }

    AnyIoExecutor executor_;
    IoStrand strand_;
    std::unordered_map<ServerSession::Key, ServerSession::Ptr> sessions_;
    SessionTimeoutScheduler::Ptr challengeTimeouts_;
    std::string logSuffix_;
    RouterContext router_;
    ServerOptions::Ptr options_;
    Listening::Ptr listener_;
    RouterLogger::Ptr logger_;
    ServerSession::Key nextSessionIndex_ = 0;

    friend class ServerContext;
};


//******************************************************************************
// ServerContext
//******************************************************************************

inline ServerContext::ServerContext(RouterContext r,
                                    const std::shared_ptr<RouterServer>& s)
    : Base(std::move(r)),
      server_(s)
{}

inline void
ServerContext::removeSession(ServerSessionKey key)
{
    auto server = server_.lock();
    if (server)
        server->removeSession(key);
}

inline void ServerContext::armChallengeTimeout(ServerSessionKey key)
{
    auto server = server_.lock();
    if (server)
        server->armChallengeTimeout(key);
}

inline void ServerContext::cancelChallengeTimeout(ServerSessionKey key)
{
    auto server = server_.lock();
    if (server)
        server->cancelChallengeTimeout(key);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
