/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DIRECTPEER_HPP
#define CPPWAMP_INTERNAL_DIRECTPEER_HPP

#include <atomic>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include "../codec.hpp"
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../errorinfo.hpp"
#include "../logging.hpp"
#include "../sessioninfo.hpp"
#include "../transport.hpp"
#include "../variant.hpp"
#include "../version.hpp"
#include "../wampdefs.hpp"
#include "commandinfo.hpp"
#include "message.hpp"
#include "realmsession.hpp"
#include "routercontext.hpp"

namespace wamp
{

namespace internal
{

class DirectPeer;

//------------------------------------------------------------------------------
template <typename TPeer>
class DirectRealmSession : public RealmSession
{
public:
    using Ptr = std::shared_ptr<DirectRealmSession>;
    using WeakPtr = std::weak_ptr<DirectRealmSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    DirectRealmSession(TPeer& peer) : peer_(peer) {}

private:
    void abort(Reason r) override
    {
        peer_.onAbort(std::move(r));
    };

    void sendError(Error&& e, bool logOnly) override
    {
        peer_.onError(std::move(e), logOnly);
    }

    void sendSubscribed(Subscribed&& s) override
    {
        peer_.onCommand(std::move(s));
    }

    void sendUnsubscribed(Unsubscribed&& u, Uri&& topic) override
    {
        peer_.onCommand(std::move(u), std::move(topic));
    }

    void sendPublished(Published&& p) override
    {
        peer_.onCommand(std::move(p));
    }

    void sendEvent(Event&& e, Uri topic) override
    {
        peer_.onCommand(std::move(e), std::move(topic));
    }

    void sendRegistered(Registered&& r) override
    {
        peer_.onCommand(std::move(r));
    }

    void sendUnregistered(Unregistered&& u, Uri&& procedure) override
    {
        peer_.onCommand(std::move(u), std::move(procedure));
    }

    void sendResult(Result&& r) override
    {
        peer_.onCommand(std::move(r));
    }

    void sendInterruption(Interruption&& i) override
    {
        peer_.onCommand(std::move(i));
    }

    void onSendInvocation(Invocation&& i) override
    {
        peer_.onCommand(std::move(i));
    }

private:
    TPeer& peer_;
};


//------------------------------------------------------------------------------
// Dummy transport providing a RouterImpl pointer for use in DirectPeer.
//------------------------------------------------------------------------------
class DirectTransport : public Transporting
{
public:
    using Ptr            = std::shared_ptr<DirectTransport>;
    using RxHandler      = typename Transporting::RxHandler;
    using TxErrorHandler = typename Transporting::TxErrorHandler;
    using PingHandler    = typename Transporting::PingHandler;

    static Ptr create(RouterContext r, std::string endpointLabel = "direct")
    {
        return Ptr(new DirectTransport(std::move(r), std::move(endpointLabel)));
    }

    TransportInfo info() const override {return {0, 0, 0};}

    bool isStarted() const override {return true;}

    void start(RxHandler, TxErrorHandler) override {}

    void send(MessageBuffer) override {}

    void sendNowAndClose(MessageBuffer message) override {}

    void close() override {}

    void ping(MessageBuffer, PingHandler) override {}

    virtual std::string remoteEndpointLabel() override {return label_;}

    RouterContext& router() {return router_;}

private:
    using Base = Transporting;

    DirectTransport(RouterContext&& r, std::string endpointLabel)
        : router_(std::move(r)),
          label_(std::move(endpointLabel))
    {}

    RouterContext router_;
    std::string label_;
};


//------------------------------------------------------------------------------
// Provides a direct link to router.
//------------------------------------------------------------------------------
class DirectPeer : public std::enable_shared_from_this<DirectPeer>
{
public:
    using State                 = SessionState;
    using InboundMessageHandler = std::function<void (Message)>;
    using LogHandler            = std::function<void (LogEntry)>;
    using StateChangeHandler    = std::function<void (State, std::error_code)>;

    DirectPeer()
        : session_(std::make_shared<DirectSessionType>(*this)),
          state_(State::disconnected),
          logLevel_(LogLevel::warning)
    {
        sessionInfo_.endpoint = "direct";
        sessionInfo_.serverName = "direct";
        sessionInfo_.agent = Version::agentString();
        sessionInfo_.serverSessionIndex = 0;
        sessionInfo_.wampSessionId = 0;
        routerLogSuffix_ = " [Direct Session]";
    }

    virtual ~DirectPeer()
    {
        realm_.leave(session_->wampId());
    }

    State state() const {return state_.load();}

    void setInboundMessageHandler(InboundMessageHandler f)
    {
        inboundMessageHandler_ = std::move(f);
    }

    void listenLogged(LogHandler handler) {logHandler_ = std::move(handler);}

    void setLogLevel(LogLevel level) {logLevel_ = level;}

    LogLevel logLevel() const
    {
        return logHandler_ ? logLevel_.load() : LogLevel::off;
    }

    void log(LogEntry entry)
    {
        if (!logHandler_)
            routerLog(std::move(entry));
        else if (logLevel() <= entry.severity())
            logHandler_(std::move(entry));
    }

    void listenStateChanged(StateChangeHandler handler)
    {
        stateChangeHandler_ = std::move(handler);
    }

    bool startConnecting()
    {
        return compareAndSetState(State::disconnected, State::connecting);
    }

    void failConnecting(std::error_code ec) {setState(State::failed, ec);}

    void connect(Transporting::Ptr transport, AnyBufferCodec)
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            setState(State::connecting);
        assert(state() == State::connecting);

        router_ =
            std::dynamic_pointer_cast<DirectTransport>(transport)->router();
        if (router_.expired())
        {
            fail(TransportErrc::disconnected, "Router expired");
            return;
        }

        routerLogger_ = router_.logger();
        if (sessionInfo_.serverSessionIndex == 0)
        {
            auto n = router_.nextDirectSessionIndex();
            sessionInfo_.serverSessionIndex = n;
            routerLogSuffix_ = " [Session direct/ " + std::to_string(n) + ']';
        }

        setState(State::closed);
        report({AccessAction::clientConnect});
    }

    bool establishSession()
    {
        return compareAndSetState(State::closed, State::establishing);
    }

    void welcome(SessionId sid, Object opts = {})
    {
        assert(false && "DirectPeer is for clients only");
    }

    bool startShuttingDown()
    {
        return compareAndSetState(State::established, State::shuttingDown);
    }

    void close()
    {
        clearSessionInfo();
        setState(State::closed);
    }

    void disconnect()
    {
        clearSessionInfo();
        auto oldState = setState(State::disconnected);
        if (oldState == State::established || oldState == State::shuttingDown)
            realm_.leave(session_->wampId());
        report({AccessAction::clientDisconnect});
        router_.reset();
        routerLogger_.reset();
    }

    ErrorOrDone send(Realm&& hello)
    {
        assert(state() == State::establishing);
        report(hello.info());
        auto realm = router_.realmAt(hello.uri());
        if (realm.expired())
            return fail(WampErrc::noSuchRealm);
        realm_ = std::move(realm);
        if (!realm_.join(session_))
            return fail(WampErrc::noSuchRealm);
        setSessionInfo(hello);
        sessionInfo_.wampSessionId = session_->wampId();
        setState(State::established);
        return true;
    }

    ErrorOrDone send(Reason&& goodbye)
    {
        assert(state() == State::shuttingDown);
        report(goodbye.info(false));
        realm_.leave(session_->wampId());
        realm_.reset();
        setState(State::closed);
        return true;
    }

    template <typename TCommand>
    ErrorOrDone send(TCommand&& cmd)
    {
        report(cmd.info(false));
        traceTx(cmd.message({}));
        if (!realm_.send(session_, std::move(cmd)))
            return fail(WampErrc::noSuchRealm);
        return true;
    }

    ErrorOrDone abort(Reason r)
    {
        report({AccessAction::clientAbort, {}, std::move(r.options()),
                r.uri()});
        bool ready = readyToAbort();
        disconnect();
        if (!ready)
            return makeUnexpectedError(Errc::invalidState);
        return true;
    }

private:
    using DirectSessionType = DirectRealmSession<DirectPeer>;

    static const std::string& stateLabel(State state)
    {
        static const std::string labels[] = {
            "DISCONNECTED", "CONNECTING", "CLOSED", "ESTABLISHING",
            "AUTHENTICATING", "ESTABLISHED", "SHUTTING_DOWN", "FAILED"};

        using Index = std::underlying_type<State>::type;
        auto n = static_cast<Index>(state);
        assert(n < Index(std::extent<decltype(labels)>::value));
        return labels[n];
    }

    State setState(State s, std::error_code ec = {})
    {
        auto old = state_.exchange(s);
        if ((old != s) && stateChangeHandler_)
            stateChangeHandler_(s, ec);
        return old;
    }

    template <typename TErrc>
    State setState(State s, TErrc errc)
    {
        return setState(s, make_error_code(errc));
    }

    bool compareAndSetState(State expected, State desired)
    {
        bool ok = state_.compare_exchange_strong(expected, desired);
        if (ok)
            stateChangeHandler_(desired, std::error_code{});
        return ok;
    }

    void setSessionInfo(Realm& hello)
    {
        sessionInfo_.realmUri = std::move(hello.uri({}));
        sessionInfo_.authId = hello.authId().value_or("");
        sessionInfo_.wampSessionId = session_->wampId();
    }

    void clearSessionInfo()
    {
        sessionInfo_.realmUri.clear();
        sessionInfo_.authId.clear();
        sessionInfo_.wampSessionId = 0;
    }

    void onAbort(Reason&& r)
    {
        auto s = state();

        if (s == State::establishing || s == State::authenticating)
        {
            setState(State::closed);
            inboundMessageHandler_(std::move(r.message({})));
        }
        else
        {
            WampErrc errc = r.errorCode();
            if (logLevel() <= LogLevel::critical)
            {
                std::ostringstream oss;
                oss << "Session aborted by peer with reason URI "
                    << r.uri();
                if (!r.options().empty())
                    oss << " and details " << r.options();
                fail(errc, oss.str());
            }
            else
            {
                fail(errc);
            }
        }
    };

    void onError(Error&& error, bool logOnly)
    {
        report(error.info(true));
        if (!logOnly)
            notifyCommand(std::move(error));
    }

    template <typename TCommand, typename... Ts>
    void onCommand(TCommand&& cmd, Ts&&... infoArgs)
    {
        report(cmd.info(true, std::forward<Ts>(infoArgs)...));
        notifyCommand(std::move(cmd));
    }

    template <typename TCommand>
    void notifyCommand(TCommand&& cmd)
    {
        if (inboundMessageHandler_ && (state() == State::established))
            inboundMessageHandler_(std::move(cmd.message({})));
    }

    void routerLog(LogEntry entry)
    {
        if (routerLogger_)
        {
            entry.append(routerLogSuffix_);
            routerLogger_->log(std::move(entry));
        }
    }

    void report(AccessActionInfo i)
    {
        if (!routerLogger_)
            return;
        routerLogger_->log(AccessLogEntry{sessionInfo_, std::move(i)});
    }

    UnexpectedError fail(std::error_code ec, std::string info = {})
    {
        setState(State::failed, ec);
        realm_.leave(session_->wampId());
        if (!info.empty())
            log({LogLevel::critical, std::move(info), ec});
        return UnexpectedError(ec);
    }

    template <typename TErrc>
    UnexpectedError fail(TErrc errc, std::string info = {})
    {
        return fail(make_error_code(errc), std::move(info));
    }

    bool readyToAbort() const
    {
        auto s = state();
        return s == State::establishing ||
               s == State::authenticating ||
               s == State::established;
    }

    void traceRx(const Array& fields)
    {
        trace(Message::parseMsgType(fields), fields, "RX");
    }

    void traceTx(const Message& msg)
    {
        trace(msg.kind(), msg.fields(), "TX");
    }

    void trace(MessageKind type, const Array& fields, const char* label)
    {
        if (logLevel() > LogLevel::trace)
            return;

        std::ostringstream oss;
        oss << "[\"" << label << "\",\""
            << MessageTraits::lookup(type).nameOr("INVALID") << "\"";
        if (!fields.empty())
            oss << "," << fields;
        oss << ']';

        LogEntry entry{LogLevel::trace, oss.str()};
        logHandler_(std::move(entry));
    }

    AccessSessionInfo sessionInfo_;
    std::string routerLogSuffix_;
    InboundMessageHandler inboundMessageHandler_;
    LogHandler logHandler_;
    StateChangeHandler stateChangeHandler_;
    DirectSessionType::Ptr session_;
    RouterContext router_;
    RouterLogger::Ptr routerLogger_;
    RealmContext realm_;
    std::atomic<State> state_;
    std::atomic<LogLevel> logLevel_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIRECTPEER_HPP
