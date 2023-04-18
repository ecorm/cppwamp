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
class DirectRouterSession : public RouterSession
{
public:
    using Ptr = std::shared_ptr<DirectRouterSession>;
    using WeakPtr = std::weak_ptr<DirectRouterSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    DirectRouterSession(DirectPeer& peer);

    using RouterSession::setRouterLogger;
    using RouterSession::routerLogLevel;
    using RouterSession::routerLog;
    using RouterSession::setTransportInfo;
    using RouterSession::setHelloInfo;
    using RouterSession::setWelcomeInfo;
    using RouterSession::resetSessionInfo;

private:
    void onRouterAbort(Reason&& r) override;
    void onRouterCommand(Error&& e) override;
    void onRouterCommand(Subscribed&& s) override;
    void onRouterCommand(Unsubscribed&& u) override;
    void onRouterCommand(Published&& p) override;
    void onRouterCommand(Event&& e) override;
    void onRouterCommand(Registered&& r) override;
    void onRouterCommand(Unregistered&& u) override;
    void onRouterCommand(Result&& r) override;
    void onRouterCommand(Interruption&& i) override;
    void onRouterCommand(Invocation&& i) override;

    DirectPeer& peer_;
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
        : session_(std::make_shared<DirectRouterSession>(*this)),
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

    void failConnecting(std::error_code ec)
    {
        setState(State::failed);
        listener_.onPeerFailure(ec, false);
    }

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

    State setState(State s)
    {
        return state_.exchange(s);
    }

    bool compareAndSetState(State expected, State desired)
    {
        return state_.compare_exchange_strong(expected, desired);
    }

    void onAbort(Reason&& reason)
    {
        traceRx(reason);
        setState(State::failed);
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
        setState(State::failed);
        realm_.leave(session_->wampId());
        listener_.onPeerFailure(ec, false, std::move(why));
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

        listener_.onPeerTrace(oss.str());
    }

    DirectRouterSession::Ptr session_;
    RouterContext router_;
    RealmContext realm_;
    PeerListener& listener_;
    std::atomic<State> state_;

    friend class DirectRouterSession;
};


//******************************************************************************
/** DirectRouterSession member function definitions. */
//******************************************************************************

inline DirectRouterSession::DirectRouterSession(DirectPeer& peer) : peer_(peer) {}
inline void DirectRouterSession::onRouterAbort(Reason&& r)         {peer_.onAbort(std::move(r));};
inline void DirectRouterSession::onRouterCommand(Error&& e)        {peer_.onCommand(std::move(e));}
inline void DirectRouterSession::onRouterCommand(Subscribed&& s)   {peer_.onCommand(std::move(s));}
inline void DirectRouterSession::onRouterCommand(Unsubscribed&& u) {peer_.onCommand(std::move(u));}
inline void DirectRouterSession::onRouterCommand(Published&& p)    {peer_.onCommand(std::move(p));}
inline void DirectRouterSession::onRouterCommand(Event&& e)        {peer_.onCommand(std::move(e));}
inline void DirectRouterSession::onRouterCommand(Registered&& r)   {peer_.onCommand(std::move(r));}
inline void DirectRouterSession::onRouterCommand(Unregistered&& u) {peer_.onCommand(std::move(u));}
inline void DirectRouterSession::onRouterCommand(Result&& r)       {peer_.onCommand(std::move(r));}
inline void DirectRouterSession::onRouterCommand(Interruption&& i) {peer_.onCommand(std::move(i));}
inline void DirectRouterSession::onRouterCommand(Invocation&& i)   {peer_.onCommand(std::move(i));}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIRECTPEER_HPP
