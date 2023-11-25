/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_PEER_HPP
#define CPPWAMP_INTERNAL_PEER_HPP

#include <array>
#include <atomic>
#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include "../any.hpp"
#include "../anyhandler.hpp"
#include "../calleestreaming.hpp"
#include "../callerstreaming.hpp"
#include "../clientinfo.hpp"
#include "../codec.hpp"
#include "../erroror.hpp"
#include "../pubsubinfo.hpp"
#include "../rpcinfo.hpp"
#include "../transport.hpp"
#include "commandinfo.hpp"
#include "peerlistener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Abstract base class for communicating with a WAMP peer
//------------------------------------------------------------------------------
class Peer : public std::enable_shared_from_this<Peer>
{
public:
    using Ptr = std::shared_ptr<Peer>;
    using State = SessionState;
    using DisconnectHandler = AnyCompletionHandler<void (ErrorOr<bool>)>;

    virtual ~Peer() = default;

    State state() const {return state_.load();}

    void listen(PeerListener* listener) {listener_ = listener;}

    bool startConnecting()
    {
        return compareAndSetState(State::disconnected, State::connecting);
    }

    void failConnecting() {setState(State::failed);}

    void connect(Transporting::Ptr transport, AnyBufferCodec codec)
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            setState(State::connecting);
        assert(state() == State::connecting);
        setState(State::closed);
        onConnect(std::move(transport), std::move(codec));
    }

    void connect(IoStrand strand, any link)
    {
        assert(state() == State::disconnected);
        setState(State::closed);
        onDirectConnect(std::move(strand), std::move(link));
    }

    bool establishSession()
    {
        if (!compareAndSetState(State::closed, State::establishing))
            return false;
        return true;
    }

    void welcome(SessionId sid, Object opts = {})
    {
        assert(isRouter_);
        assert(state() == State::authenticating);
        send(Welcome{{}, sid, std::move(opts)});
        setState(State::established);
    }

    bool startShuttingDown()
    {
        return compareAndSetState(State::established, State::shuttingDown);
    }

    void close()
    {
        auto oldState = setState(State::closed);
        if (oldState == State::established)
            send(Goodbye{WampErrc::goodbyeAndOut});
        onClose();
    }

    void disconnect()
    {
        auto oldState = setState(State::disconnected);
        onDisconnect(oldState);
    }

    void disconnectGracefully(DisconnectHandler handler)
    {
        auto oldState = setState(State::disconnecting);
        onDisconnectGracefully(oldState, std::move(handler));
    }

    void fail()
    {
        auto oldState = setState(State::failed);
        onDisconnect(oldState);
    }

    virtual ErrorOrDone abort(Abort) = 0;

    virtual ErrorOrDone send(Error&&) = 0;

    virtual ErrorOrDone send(Goodbye&&) = 0;
    virtual ErrorOrDone send(Petition&&) = 0;
    virtual ErrorOrDone send(Welcome&&) = 0;
    virtual ErrorOrDone send(Authentication&&) = 0;
    virtual ErrorOrDone send(Challenge&&) = 0;

    virtual ErrorOrDone send(Topic&&) = 0;
    virtual ErrorOrDone send(Pub&&) = 0;
    virtual ErrorOrDone send(Event&&) = 0;
    virtual ErrorOrDone send(Subscribed&&) = 0;
    virtual ErrorOrDone send(Unsubscribe&&) = 0;
    virtual ErrorOrDone send(Unsubscribed&&) = 0;
    virtual ErrorOrDone send(Published&&) = 0;

    virtual ErrorOrDone send(Procedure&&) = 0;
    virtual ErrorOrDone send(Rpc&&) = 0;
    virtual ErrorOrDone send(Result&&) = 0;
    virtual ErrorOrDone send(Invocation&&) = 0;
    virtual ErrorOrDone send(CallCancellation&&) = 0;
    virtual ErrorOrDone send(Interruption&&) = 0;
    virtual ErrorOrDone send(Registered&&) = 0;
    virtual ErrorOrDone send(Unregister&&) = 0;
    virtual ErrorOrDone send(Unregistered&&) = 0;

    virtual ErrorOrDone send(Stream&&) = 0;
    virtual ErrorOrDone send(StreamRequest&&) = 0;
    virtual ErrorOrDone send(CalleeOutputChunk&&) = 0;
    virtual ErrorOrDone send(CallerOutputChunk&&) = 0;

protected:
    static const std::string& stateLabel(State state)
    {
        static const std::array<std::string, 8> labels{{
            "DISCONNECTED", "CONNECTING", "CLOSED", "ESTABLISHING",
            "AUTHENTICATING", "ESTABLISHED", "SHUTTING_DOWN", "FAILED"}};

        using Index = std::underlying_type<State>::type;
        auto n = static_cast<Index>(state);
        assert(n >= 0);
        return labels.at(n);
    }

    explicit Peer(bool isRouter)
        : state_(State::disconnected),
          isRouter_(isRouter)
    {}

    virtual void onConnect(Transporting::Ptr, AnyBufferCodec) = 0;

    virtual void onDirectConnect(IoStrand, any) = 0;

    virtual void onClose() = 0;

    virtual void onDisconnect(State previousState) = 0;

    virtual void onDisconnectGracefully(State previousState,
                                        DisconnectHandler handler) = 0;

    State setState(State s) {return state_.exchange(s);}

    bool compareAndSetState(State expected, State desired)
    {
        return state_.compare_exchange_strong(expected, desired);
    }

    PeerListener& listener() {return *listener_;}

    void traceRx(const Message& msg)
    {
        trace(msg.kind(), msg.fields(), "RX");
    }

    void traceRx(const Array& fields)
    {
        trace(Message::parseMsgType(fields), fields, "RX");
    }

    void traceTx(const Message& msg)
    {
        trace(msg.kind(), msg.fields(), "TX");
    }

    bool isRouter() const {return isRouter_;}

private:
    void trace(MessageKind kind, const Array& fields, const char* label)
    {
        if (!listener_->traceEnabled())
            return;

        std::ostringstream oss;
        const char* name = MessageTraits::lookup(kind).name;
        oss << "[\"" << label << "\",\"" << name << "\"";
        if (!fields.empty())
            oss << "," << fields;
        oss << ']';

        listener_->onPeerTrace(oss.str());
    }

    PeerListener* listener_ = nullptr;
    std::atomic<State> state_;
    bool isRouter_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_PEER_HPP
