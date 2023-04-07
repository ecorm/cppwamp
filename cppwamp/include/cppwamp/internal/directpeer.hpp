/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DIRECTPEER_HPP
#define CPPWAMP_INTERNAL_DIRECTPEER_HPP

#include <atomic>
#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include "../codec.hpp"
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../transport.hpp"
#include "message.hpp"
#include "peerlistener.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{

class DirectPeer;

//------------------------------------------------------------------------------
template <typename TPeer>
class DirectRouterSession : public RouterSession
{
public:
    using Ptr = std::shared_ptr<DirectRouterSession>;
    using WeakPtr = std::weak_ptr<DirectRouterSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    DirectRouterSession(TPeer& peer) : peer_(peer) {}

    using RouterSession::setRouterLogger;
    using RouterSession::routerLogLevel;
    using RouterSession::routerLog;
    using RouterSession::setTransportInfo;
    using RouterSession::setHelloInfo;
    using RouterSession::setWelcomeInfo;
    using RouterSession::resetSessionInfo;

private:
    void onRouterAbort(Reason&& r) override         {peer_.onAbort(std::move(r));};
    void onRouterCommand(Error&& e) override        {peer_.onCommand(std::move(e));}
    void onRouterCommand(Subscribed&& s) override   {peer_.onCommand(std::move(s));}
    void onRouterCommand(Unsubscribed&& u) override {peer_.onCommand(std::move(u));}
    void onRouterCommand(Published&& p) override    {peer_.onCommand(std::move(p));}
    void onRouterCommand(Event&& e) override        {peer_.onCommand(std::move(e));}
    void onRouterCommand(Registered&& r) override   {peer_.onCommand(std::move(r));}
    void onRouterCommand(Unregistered&& u) override {peer_.onCommand(std::move(u));}
    void onRouterCommand(Result&& r) override       {peer_.onCommand(std::move(r));}
    void onRouterCommand(Interruption&& i) override {peer_.onCommand(std::move(i));}
    void onRouterCommand(Invocation&& i) override   {peer_.onCommand(std::move(i));}

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
    using State = SessionState;

    explicit DirectPeer(PeerListener* listener)
        : session_(std::make_shared<DirectSessionType>(*this)),
          listener_(*listener),
          state_(State::disconnected)
    {}

    virtual ~DirectPeer()
    {
        realm_.leave(session_->wampId());
    }

    State state() const {return state_.load();}

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
            fail("Router expired", TransportErrc::disconnected);
            return;
        }

        session_->setRouterLogger(router_.logger());
        auto n = router_.nextDirectSessionIndex();
        session_->setTransportInfo({"direct", "direct", n});

        setState(State::closed);
        session_->report({AccessAction::clientConnect});
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
        session_->resetSessionInfo();
        setState(State::closed);
    }

    void disconnect()
    {
        session_->resetSessionInfo();
        auto oldState = setState(State::disconnected);
        if (oldState == State::established || oldState == State::shuttingDown)
            realm_.leave(session_->wampId());
        session_->report({AccessAction::clientDisconnect});
        router_.reset();
        session_->setRouterLogger(nullptr);
    }

    ErrorOrDone send(Realm&& hello)
    {
        assert(state() == State::establishing);
        traceTx(hello);
        session_->report(hello.info());

        auto realm = router_.realmAt(hello.uri());
        bool found = false;
        if (!realm.expired())
        {
            realm_ = std::move(realm);
            found = realm_.join(session_);
        }
        if (!found)
        {
            return fail("Realm '" + hello.uri() + "' not found",
                        WampErrc::noSuchRealm);
        }

        AuthInfo authInfo
        {
            hello.authId().value_or(""),
            hello.optionOr<String>("authrole", ""),
            hello.optionOr<String>("authmethod", "x_cppwamp_direct"),
            hello.optionOr<String>("authprovider", "direct")
        };
        session_->setHelloInfo(hello);
        session_->setWelcomeInfo(std::move(authInfo));

        setState(State::established);
        return true;
    }

    ErrorOrDone send(Reason&& goodbye)
    {
        assert(state() == State::shuttingDown);
        traceTx(goodbye);
        session_->report(goodbye.info(false));
        realm_.leave(session_->wampId());
        realm_.reset();
        close();
        return true;
    }

    template <typename C>
    ErrorOrDone send(C&& command)
    {
        traceTx(command);
        session_->report(command.info(false));
        if (!realm_.send(session_, std::move(command)))
            return fail("Realm expired", WampErrc::noSuchRealm);
        return true;
    }

    ErrorOrDone abort(Reason reason)
    {
        traceTx(reason);
        session_->report(reason.info(false));
        bool ready = readyToAbort();
        disconnect();
        if (!ready)
            return makeUnexpectedError(Errc::invalidState);
        return true;
    }

private:
    using DirectSessionType = DirectRouterSession<DirectPeer>;

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
        if (old != s)
            listener_.onStateChanged(s, ec);
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
            listener_.onStateChanged(desired, std::error_code{});
        return ok;
    }

    void onAbort(Reason&& reason)
    {
        traceRx(reason);
        setState(State::failed, reason.errorCode());
        listener_.onPeerAbort(std::move(reason), false);
    };

    template <typename C>
    void onCommand(C&& command)
    {
        traceRx(command);
        listener_.onPeerCommand(std::move(command));
    }

    UnexpectedError fail(std::string why, std::error_code ec)
    {
        setState(State::failed, ec);
        realm_.leave(session_->wampId());
        listener_.onFailure(std::move(why), ec, false);
        return UnexpectedError(ec);
    }

    template <typename TErrc>
    UnexpectedError fail(std::string why, TErrc errc)
    {
        return fail(std::move(why), make_error_code(errc));
    }

    bool readyToAbort() const
    {
        auto s = state();
        return s == State::establishing ||
               s == State::authenticating ||
               s == State::established;
    }

    template <typename C>
    void traceRx(const C& command)
    {
        trace(command.message({}), "RX");
    }

    template <typename C>
    void traceTx(const C& command)
    {
        trace(command.message({}), "TX");
    }

    void trace(const Message& message, const char* label)
    {
        if (!listener_.traceEnabled())
            return;

        std::ostringstream oss;
        oss << "[\"" << label << "\",\"" << message.name() << "\"";
        const auto& fields = message.fields();
        if (!fields.empty())
            oss << "," << fields;
        oss << ']';

        listener_.onTrace(oss.str());
    }

    DirectSessionType::Ptr session_;
    RouterContext router_;
    RealmContext realm_;
    PeerListener& listener_;
    std::atomic<State> state_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIRECTPEER_HPP
