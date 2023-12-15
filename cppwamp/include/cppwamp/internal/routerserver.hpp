/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
#define CPPWAMP_INTERNAL_ROUTER_SERVER_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "../routeroptions.hpp"
#include "../authenticators/anonymousauthenticator.hpp"
#include "challenger.hpp"
#include "commandinfo.hpp"
#include "cppwamp/internal/networkpeer.hpp"
#include "networkpeer.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

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
    using TimePoint = std::chrono::steady_clock::time_point;

    ServerSession(AnyIoExecutor e, Transporting::Ptr&& t, ServerContext&& s,
                  ServerOptions::Ptr o, Key k)
        : Base(s.logger()),
          executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          peer_(std::make_shared<NetworkPeer>(true)),
          transport_(t),
          server_(std::move(s)),
          serverOptions_(std::move(o)),
          uriValidator_(server_.uriValidator()),
          key_(k)
    {
        assert(serverOptions_ != nullptr);
        auto info = t->connectionInfo();
        info.setServer({}, serverOptions_->name(), k);
        Base::connect(std::move(info));
        bumpLastActivityTime();
        peer_->listen(this);
    }

    ~ServerSession() override = default;

    Key index() const {return key_;}

    void start()
    {
        auto self = shared_from_this();
        dispatch([this, self]() {startSession();});
    }

    void monitor()
    {
        auto self = shared_from_this();
        dispatch([this, self]() {doMonitor();});
    }

    void onChallengeTimeout()
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->reject(Abort{WampErrc::timeout});}
        };

        safelyDispatch<Dispatched>();
    }

    TimePoint lastActivityTime() const
    {
        return TimePoint{Timeout{lastActivityTime_.load()}};
    }

    ServerSession(const ServerSession&) = delete;
    ServerSession(ServerSession&&) = delete;
    ServerSession& operator=(const ServerSession&) = delete;
    ServerSession& operator=(ServerSession&&) = delete;

private:
    using Base = RouterSession;

    static TimePoint steadyTime() {return std::chrono::steady_clock::now();}

    void onRouterAbort(Abort&& reason) override
    {
        struct Dispatched
        {
            Ptr self;
            Abort reason;
            void operator()() {self->abortSession(std::move(reason));}
        };

        safelyDispatch<Dispatched>(std::move(reason));
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

        bumpLastActivityTime();
        dispatch(Dispatched{shared_from_this(), std::move(msg)});
    }

    void onPeerDisconnect() override
    {
        report({AccessAction::clientDisconnect});
        retire();
    }

    void onPeerFailure(std::error_code ec, std::string why,
                       bool abortNeeded) override
    {
        auto action = abortNeeded ? AccessAction::serverAbort
                                  : AccessAction::serverDisconnect;
        Object opts;
        if (!why.empty())
            opts.emplace("message", why);
        report({action, {}, std::move(opts), ec});

        if (!abortNeeded)
            return retire();

        leaveRealm();
        Abort reason{ec};
        if (!why.empty())
            reason.withHint(std::move(why));
        auto self = shared_from_this();
        peer_->abort(
            std::move(reason),
            [this, self](ErrorOr<bool> done)
            {
                retire();
            });
    }

    void onPeerTrace(std::string&& messageDump) override
    {
        Base::routerLog({LogLevel::trace, std::move(messageDump)});
    }

    void onPeerHello(Hello&& hello) override
    {
        Base::report(hello.info());
        bumpLastActivityTime();
        helloDeadline_ = TimePoint::max();

        realm_ = server_.realmAt(hello.uri());
        if (realm_.expired())
        {
            auto errc = WampErrc::noSuchRealm;
            abortSession({errc});
            return;
        }

        authExchange_ = AuthExchange::create({}, std::move(hello),
                                             shared_from_this());
        serverOptions_->authenticator()->authenticate(authExchange_, executor_);
    }

    void onPeerAbort(Abort&& reason, bool) override
    {
        report(reason.info(false));
        retire();
    }

    void onPeerChallenge(Challenge&&) override {assert(false);}

    void onPeerAuthenticate(Authentication&& authentication) override
    {
        Base::report(authentication.info());
        bumpLastActivityTime();

        const bool isExpected = authExchange_ != nullptr &&
                                state() == State::authenticating;
        if (!isExpected)
        {
            return abortSession(Abort(WampErrc::protocolViolation).
                                withHint("Unexpected AUTHENTICATE message"));
        }

        challengeDeadline_ = TimePoint::max();
        authExchange_->setAuthentication({}, std::move(authentication));
        serverOptions_->authenticator()->authenticate(authExchange_, executor_);
    }

    void onPeerGoodbye(Goodbye&& reason, bool wasShuttingDown) override
    {
        report(reason.info(false));

        if (!uriValidator_->checkError(reason.uri()))
            return abortSession(Abort(WampErrc::invalidUri));

        if (!wasShuttingDown)
        {
            report({AccessAction::serverGoodbye,
                    errorCodeToUri(WampErrc::goodbyeAndOut)});
            peer_->close();
        }

        leaveRealm();
        if (wasShuttingDown)
            return;

        peer_->establishSession();

        auto timeout = serverOptions_->helloTimeout();
        if (timeoutIsDefinite(timeout))
            helloDeadline_ = steadyTime() + timeout;
    }

    void onPeerMessage(Message&& m) override
    {
        bumpLastActivityTime();

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

    void bumpLastActivityTime()
    {
        lastActivityTime_.store(steadyTime().time_since_epoch().count());
    }

    State state() const {return peer_->state();}

    void startSession()
    {
        assert(!alreadyStarted_);
        alreadyStarted_ = true;

        const std::weak_ptr<ServerSession> self = shared_from_this();
        transport_->admit(
            [self](AdmitResult result)
            {
                auto me = self.lock();
                if (!me)
                    return;
                me->onPeerAdmitted(result);
            });
    }

    void onPeerAdmitted(AdmitResult result)
    {
        using S = AdmitStatus;

        switch (result.status())
        {
        case S::responded:
            shutdownTransportThenRetire();
            break;

        case S::wamp:
            onPeerNegotiated(result.codecId());
            break;

        case S::rejected:
            Base::report({AccessAction::serverReject, result.error()});
            shutdownTransportThenRetire(result.error());
            break;

        case S::failed:
            Base::routerLog(
                {LogLevel::error,
                 std::string{"Handshake failure during "} + result.operation(),
                 result.error()});
            retire();
            break;

        default:
            assert(false && "Unexpected AdmitStatus enumerator");
            break;;
        }
    }

    void shutdownTransportThenRetire(std::error_code reason = {})
    {
        auto self = shared_from_this();
        transport_->shutdown(
            reason,
            [this, self](std::error_code shutdownEc)
            {
                if (shutdownEc)
                    Base::report({AccessAction::serverDisconnect, shutdownEc});
                retire();
            });
    }

    void onPeerNegotiated(int codecId)
    {
        if (routerLogLevel() == LogLevel::trace)
            enableTracing();
        auto codec = serverOptions_->makeCodec({}, codecId);
        assert(static_cast<bool>(codec));
        peer_->connect(std::move(transport_), std::move(codec));
        peer_->establishSession();
        report({AccessAction::clientConnect});

        auto timeout = serverOptions_->helloTimeout();
        if (timeoutIsDefinite(timeout))
            helloDeadline_ = steadyTime() + timeout;
    }

    void doMonitor()
    {
        if (transport_ == nullptr)
            monitorPeerTransport();
        else
            monitorTransport();
    }

    void monitorPeerTransport()
    {
        std::error_code ec = peer_->monitor();

        if (ec == TransportErrc::lingerTimeout)
        {
            report({AccessAction::serverDisconnect, ec});
            peer_->disconnect();
            return retire();
        }

        if (!ec)
        {
            auto now = steadyTime();
            if (now >= helloDeadline_)
                ec = make_error_code(ServerErrc::helloTimeout);
            else if (now >= challengeDeadline_)
                ec = make_error_code(ServerErrc::challengeTimeout);
        }

        if (!ec)
            return;

        auto hint = detailedErrorCodeString(ec);
        abortSession(Abort{WampErrc::sessionKilled}.withHint(std::move(hint)),
                     {AccessAction::serverAbort, ec});
    }

    void monitorTransport()
    {
        std::error_code ec = transport_->monitor();
        if (!ec)
            return;

        report({AccessAction::serverDisconnect, ec});
        transport_->close();
        retire();
    }

    void onAdmitError(std::error_code ec, const char* operation)
    {
        report({AccessAction::serverReject, ec});
        retire();
    }

    void abortSession(Abort reason)
    {
        AccessActionInfo a{AccessAction::serverAbort, {}, reason.options(),
                           reason.uri()};
        abortSession(std::move(reason), std::move(a));
    }

    void abortSession(Abort reason, AccessActionInfo a)
    {
        report(std::move(a));

        auto self = shared_from_this();
        peer_->abort(
            std::move(reason),
            [this, self](ErrorOr<bool> done)
            {
                retire();
            });

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
            abortSession(Abort(WampErrc::protocolViolation)
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

            auto timeout = serverOptions_->challengeTimeout();
            if (timeoutIsDefinite(timeout))
                challengeDeadline_ = steadyTime() + timeout;
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
        if (!serverOptions_->agent().empty())
            welcomeDetails["agent"] = serverOptions_->agent();
        info->setAgent(hello.agentOrEmptyString({}));
        info->setFeatures(hello.features());
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

    void reject(Abort&& reason)
    {
        authExchange_.reset();
        const auto s = state();
        const bool readyToReject = s == State::establishing ||
                                   s == State::authenticating;
        if (!readyToReject)
            return;

        close();
        report({AccessAction::serverAbort, {},
                reason.options(), reason.errorCode()});
        peer_->abort(std::move(reason));
    }

    void safeReject(Abort&& reason) override
    {
        struct Dispatched
        {
            Ptr self;
            Abort reason;
            void operator()() {self->reject(std::move(reason));}
        };

        safelyDispatch<Dispatched>(std::move(reason));
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
    ServerContext server_;
    RealmContext realm_;
    ServerOptions::Ptr serverOptions_;
    AuthExchange::Ptr authExchange_;
    RequestIdChecker requestIdChecker_;
    UriValidator::Ptr uriValidator_;
    std::atomic<TimePoint::rep> lastActivityTime_;
    Key key_;
    TimePoint helloDeadline_ = TimePoint::max();
    TimePoint challengeDeadline_ = TimePoint::max();
    bool alreadyStarted_ = false;
};

//------------------------------------------------------------------------------
class BinaryExponentialBackoffTimer
{
public:
    using Backoff = BinaryExponentialBackoff;

    template <typename E>
    BinaryExponentialBackoffTimer(E&& executor, Backoff b)
        : backoffTimer_(std::forward<E>(executor)),
          backoff_(b)
    {}

    const Backoff& backoff() const {return backoff_;}

    void cancel()
    {
        backoffTimer_.cancel();
        reset();
    }

    void reset() {backoffDelay_ = unspecifiedTimeout;}

    template <typename F>
    void wait(F&& callback)
    {
        const bool backoffInProgress = backoffDelay_ != unspecifiedTimeout;

        if (backoffInProgress)
        {
            if (backoffDelay_ > (backoff_.max() / 2))
                backoffDelay_ = backoff_.max();
            else
                backoffDelay_ *= 2;
            backoffDeadline_ += backoffDelay_;
        }
        else
        {
            backoffDelay_ = backoff_.min();
            backoffDeadline_ = Clock::now() + backoffDelay_;
        }

        backoffTimer_.expires_at(backoffDeadline_);
        backoffTimer_.async_wait(std::forward<F>(callback));
    }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    boost::asio::steady_timer backoffTimer_;
    Backoff backoff_;
    Timeout backoffDelay_ = unspecifiedTimeout;
    TimePoint backoffDeadline_;
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

    void close(Abort reason)
    {
        struct Dispatched
        {
            Ptr self;
            Abort reason;
            void operator()() {self->onClose(std::move(reason));}
        };

        safelyDispatch<Dispatched>(std::move(reason));
    }

    ServerOptions::Ptr config() const {return options_;}

private:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Backoff = BinaryExponentialBackoff;

    static TimePoint steadyTime() {return std::chrono::steady_clock::now();}

    RouterServer(AnyIoExecutor e, ServerOptions&& c, RouterContext&& r)
        : executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          backoffTimer_(strand_, c.acceptBackoff()),
          monitoringTimer_(strand_),
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
        listener_ = options_->makeListener({}, executor_, strand_, logger_);
        inform("Starting server listening on " + listener_->where());

        const std::weak_ptr<RouterServer> self{shared_from_this()};
        listener_->observe(
            [this, self](ListenResult result)
            {
                auto me = self.lock();
                if (me && listener_)
                    onListenerResult(std::move(result));
            });

        monitoringDeadline_ = steadyTime();
        monitor();
        listen();
    }

    void monitor()
    {
        monitoringDeadline_ += options_->monitoringInterval();
        const auto now = steadyTime();
        if (monitoringDeadline_ <= now)
            monitoringDeadline_ = now + options_->monitoringInterval();

        const std::weak_ptr<RouterServer> self{shared_from_this()};
        monitoringTimer_.expires_at(monitoringDeadline_);
        monitoringTimer_.async_wait(
            [this, self](boost::system::error_code ec)
            {
                auto me = self.lock();
                if (me)
                    onMonitoringTick(ec);
            });
    }

    void onChallengeTimeout(ServerSession::Key key)
    {
        auto found = sessions_.find(key);
        if (found == sessions_.end())
            return;
        found->second->onChallengeTimeout();
    }

    void onListenerResult(ListenResult result)
    {
        using S = ListenStatus;

        switch (result.status())
        {
        case S::success:
            backoffTimer_.reset();
            onAccepted();
            listen();
            break;

        case S::cancelled:
            break;

        case S::transient:
            alert(std::string("Error establishing connection with "
                              "remote peer during ") + result.operation(),
                  result.error());
            backoffTimer_.reset();
            listen();
            break;

        case S::overload:
            backOffAccept(result, "Resource exhaustion detected during ");
            break;

        case S::outage:
            backOffAccept(result, "Network outage detected during  ");
            break;

        case S::fatal:
            panic(std::string("Fatal error establishing connection with "
                              "remote peer during ") + result.operation(),
                  result.error());
            onClose(Abort{WampErrc::systemShutdown});
            break;

        default:
            assert(false && "Unexpected ListenStatus enumerator");
        }
    }

    /*  Backs off socket accept operations when resource exhaustion or
        network outage occurs to avoid flooding the log. *Not* called when the
        connection limit is reached (those connections are shedded instead). */
    void backOffAccept(const ListenResult& result, std::string why)
    {
        alert(why + result.operation(), result.error());
        if (backoffTimer_.backoff().isUnspecified())
            return listen();

        std::weak_ptr<RouterServer> self{shared_from_this()};
        backoffTimer_.wait(
            [self](boost::system::error_code ec)
            {
                auto me = self.lock();
                if (me)
                    me->onBackoffExpired(ec);
            });
    }

    void onBackoffExpired(boost::system::error_code ec)
    {
        if (ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
            panic("Accept backoff timer failure", ec);

        listen();
    }

    void onMonitoringTick(boost::system::error_code ec)
    {
        if (ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
            panic("Monitoring timer failure", ec);

        for (const auto& kv: sessions_)
            kv.second->monitor();

        monitor();
    }

    void listen()
    {
        if (!listener_)
            return;
        listener_->establish();
    }

    void onAccepted()
    {
        const auto sessionCount = sessions_.size() + sheddingTransportsCount_;

        if (sessionCount >= options_->hardConnectionLimit())
            return hardShed();

        auto transport = listener_->take();
        if (sessionCount >= options_->softConnectionLimit())
        {
            if (!softShedStaleSession())
                return softShedAccepted(std::move(transport));
        }

        ServerContext ctx{router_, shared_from_this()};
        if (++nextSessionIndex_ == 0u)
            nextSessionIndex_ = 1u;
        auto index = nextSessionIndex_;
        auto s = std::make_shared<ServerSession>(
            executor_, std::move(transport), std::move(ctx), options_, index);
        sessions_.emplace(index, s);
        s->start();
    }

    void hardShed()
    {
        warn("Dropping client connection due to hard connection limit");
        listener_->drop();
    }

    bool softShedStaleSession()
    {
        auto staleTimeout = options_->staleTimeout();
        if (!timeoutIsDefinite(staleTimeout))
            return false;

        ServerSession* stalest = nullptr;
        Timeout maxIdleTime = staleTimeout;
        auto now = steadyTime();

        for (auto& kv: sessions_)
        {
            auto& session = kv.second;
            auto idleTime = now - session->lastActivityTime();
            if (idleTime >= maxIdleTime)
            {
                maxIdleTime = idleTime;
                stalest = session.get();
            }
        }

        if (stalest == nullptr)
            return false;

        warn("Evicting stale client session due to soft connection limit");
        auto hint = detailedErrorCodeString(make_error_code(ServerErrc::evicted));
        auto reason = Abort{WampErrc::sessionKilled}.withHint(std::move(hint));
        stalest->abort(std::move(reason));
        return true;
    }

    void softShedAccepted(Transporting::Ptr transport)
    {
        ++sheddingTransportsCount_;
        auto self = shared_from_this();
        transport->shed(
            [this, self, transport](AdmitResult result)
            {
                onRefusalCompleted(*transport, result);
            });
    }

    void onRefusalCompleted(const Transporting& transport, AdmitResult result)
    {
        if (sheddingTransportsCount_ > 0)
            --sheddingTransportsCount_;

        switch (result.status())
        {
        case AdmitStatus::shedded:
            report(transport, {AccessAction::serverReject, result.error()});
            warn("Client connection refused due to soft connection limit");
            break;

        case AdmitStatus::rejected:
            report(transport, {AccessAction::serverReject, result.error()});
            warn("Client handshake rejected or timed out", result.error());
            break;

        case AdmitStatus::failed:
            report(transport, {AccessAction::serverReject, result.error()});
            alert("Error establishing connection with remote peer "
                  "during transport handshake", result.error());
            break;

        default:
            assert(false && "Unexpected AdmitResult status");
        }
    }

    void onClose(Abort reason)
    {
        std::string msg = "Shutting down server listening on " +
                          listener_->where() + " with reason " + reason.uri();
        if (!reason.options().empty())
            msg += " " + toString(reason.options());
        inform(std::move(msg));

        backoffTimer_.cancel();

        if (!listener_)
            return;
        listener_->cancel();
        listener_.reset();
        for (const auto& kv: sessions_)
            kv.second->abort(reason);
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

    void report(const Transporting& transport, AccessActionInfo&& info)
    {
        logger_->log(AccessLogEntry{transport.connectionInfo(), {},
                                    std::move(info)});
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

    void panic(String msg, std::error_code ec = {})
    {
        log({LogLevel::critical, std::move(msg), ec});
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
    BinaryExponentialBackoffTimer backoffTimer_;
    boost::asio::steady_timer monitoringTimer_;
    std::string logSuffix_;
    RouterContext router_;
    ServerOptions::Ptr options_;
    Listening::Ptr listener_;
    RouterLogger::Ptr logger_;
    ServerSession::Key nextSessionIndex_ = 0;
    TimePoint monitoringDeadline_;
    std::size_t sheddingTransportsCount_ = 0;

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

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_SERVER_HPP
