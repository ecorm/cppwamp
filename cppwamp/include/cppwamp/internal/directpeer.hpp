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
#include "../wampdefs.hpp"
#include "realmsession.hpp"
#include "routercontext.hpp"
#include "wampmessage.hpp"

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
        peer_.onMessage(e);
    }

    void sendSubscribed(RequestId r, SubscriptionId s) override
    {
        peer_.template onMessageInfo<SubscribedMessage>(r, s);
    }

    void sendUnsubscribed(RequestId r, Uri&& topic) override
    {
        peer_.template onMessageInfo<UnsubscribedMessage>(r);
    }

    void sendPublished(RequestId r, PublicationId p) override
    {
        peer_.template onMessageInfo<PublishedMessage>(r, p);
    }

    void sendEvent(Event&& e, Uri topic) override
    {
        peer_.onMessage(std::move(e));
    }

    void sendRegistered(RequestId reqId, RegistrationId regId) override
    {
        peer_.template onMessageInfo<RegisteredMessage>(reqId, regId);
    }

    void sendUnregistered(RequestId r, Uri&& procedure) override
    {
        peer_.template onMessageInfo<UnregisteredMessage>(r);
    }

    void sendResult(Result&& r) override
    {
        peer_.onMessage(std::move(r));
    }

    void sendInterruption(Interruption&& i) override
    {
        peer_.onMessage(std::move(i));
    }

    void log(LogEntry e) override
    {
        peer_.onLog(std::move(e));
    }

    void report(AccessActionInfo i) override
    {
        peer_.onReport(std::move(i));
    }

    void onSendInvocation(Invocation&& i) override
    {
        peer_.onMessage(std::move(i));
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
    using Message               = WampMessage;
    using InboundMessageHandler = std::function<void (Message)>;
    using LogHandler            = std::function<void (LogEntry)>;
    using StateChangeHandler    = std::function<void (State, std::error_code)>;

    DirectPeer()
        : session_(std::make_shared<DirectSessionType>(*this)),
          state_(State::disconnected),
          logLevel_(LogLevel::warning)
    {}

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
        if (logLevel() <= entry.severity())
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
        setState(State::closed);
    }

    bool establishSession()
    {
        return compareAndSetState(State::closed, State::establishing);
    }

    void challenge(Challenge challenge)
    {
        assert(false && "DirectPeer is for clients only");
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
        setState(State::closed);
    }

    void disconnect()
    {
        auto oldState = setState(State::disconnected);
        if (oldState == State::established || oldState == State::shuttingDown)
            realm_.leave(session_->wampId());
    }

    ErrorOrDone send(HelloMessage& m)
    {
        // TODO: Log
        assert(state() == State::establishing);
        auto realm = router_.realmAt(m.uri());
        if (realm.expired())
            return fail(WampErrc::noSuchRealm);
        realm_ = std::move(realm);
        if (!realm_.join(session_))
            return fail(WampErrc::noSuchRealm);
        return true;
    }

    ErrorOrDone send(GoodbyeMessage&)
    {
        // TODO: Log
        assert(state() == State::shuttingDown);
        realm_.leave(session_->wampId());
        realm_.reset();
        setState(State::closed);
        return true;
    }

    ErrorOrDone send(SubscribeMessage& msg)
    {
        if (!realm_.subscribe(session_, Topic{{}, std::move(msg)}))
            return fail(WampErrc::noSuchRealm);
        traceTx(msg);
        return true;
    }

    ErrorOrDone send(UnsubscribeMessage& m)
    {
        if (!realm_.unsubscribe(session_, m.subscriptionId(), m.requestId()))
            return fail(WampErrc::noSuchRealm);
        traceTx(m);
        return true;
    }

    ErrorOrDone send(PublishMessage& m)
    {
        if (!realm_.publish(session_, Pub{{}, std::move(m)}))
            return fail(WampErrc::noSuchRealm);
        traceTx(m);
        return true;
    }

    ErrorOrDone send(RegisterMessage& m)
    {
        if (!realm_.enroll(session_, Procedure{{}, std::move(m)}))
            return fail(WampErrc::noSuchRealm);
        traceTx(m);
        return true;
    }

    ErrorOrDone send(UnregisterMessage& m)
    {
        if (!realm_.unregister(session_, m.registrationId(), m.requestId()))
            return fail(WampErrc::noSuchRealm);
        traceTx(m);
        return true;
    }

    ErrorOrDone send(CallMessage& m)
    {
        if (!realm_.call(session_, Rpc{{}, std::move(m)}))
            return fail(WampErrc::noSuchRealm);
        traceTx(m);
        return true;
    }

    ErrorOrDone send(CancelMessage& m)
    {
        if (!realm_.cancelCall(session_, CallCancellation{{}, std::move(m)}))
            return fail(WampErrc::noSuchRealm);
        traceTx(m);
        return true;
    }

    ErrorOrDone send(YieldMessage& m)
    {
        if (!realm_.yieldResult(session_, Result{{}, std::move(m)}))
            return fail(WampErrc::noSuchRealm);
        traceTx(m);
        return true;
    }

    ErrorOrDone sendError(WampMsgType reqType, RequestId reqId, Error&& error)
    {
        // TODO: Avoid intermediate message object
        auto& msg = error.errorMessage({}, reqType, reqId);
        if (!realm_.yieldError(session_, Error{{}, std::move(msg)}))
            return fail(WampErrc::noSuchRealm);
        traceTx(msg);
        return true;
    }

    ErrorOrDone abort(Reason r)
    {
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

    void onAbort(Reason&& r)
    {
        auto s = state();
        WampErrc errc = r.errorCode();

        if (s == State::establishing || s == State::authenticating)
        {
            setState(State::closed);
            inboundMessageHandler_(r.abortMessage({}));
        }
        else
        {
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

    template <typename TMessage, typename... TArgs>
    void onMessageInfo(TArgs&&... args)
    {
        if (inboundMessageHandler_ && (state() == State::established))
            inboundMessageHandler_(TMessage{std::forward<TArgs>(args)...});
    }

    void onMessage(WampMessage&& msg)
    {
        if (inboundMessageHandler_ && (state() == State::established))
            inboundMessageHandler_(std::move(msg));
    }

    void onLog(LogEntry e)
    {
        // TODO
    }

    void onReport(AccessActionInfo i)
    {
        // TODO
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
        trace(msg.type(), msg.fields(), "TX");
    }

    void trace(WampMsgType type, const Array& fields, const char* label)
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

    InboundMessageHandler inboundMessageHandler_;
    LogHandler logHandler_;
    StateChangeHandler stateChangeHandler_;
    DirectSessionType::Ptr session_;
    RouterContext router_;
    RealmContext realm_;
    std::atomic<State> state_;
    std::atomic<LogLevel> logLevel_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIRECTPEER_HPP
