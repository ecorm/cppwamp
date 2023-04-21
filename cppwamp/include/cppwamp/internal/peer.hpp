/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_PEER_HPP
#define CPPWAMP_INTERNAL_PEER_HPP

#include <atomic>
#include <cassert>
#include <memory>
#include <utility>
#include "../calleestreaming.hpp"
#include "../callerstreaming.hpp"
#include "../codec.hpp"
#include "../erroror.hpp"
#include "../transport.hpp"
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

    virtual ~Peer()
    {
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    State state() const {return state_.load();}

    void listen(PeerListener* listener) {listener_ = listener;}

    bool startConnecting()
    {
        return compareAndSetState(State::disconnected, State::connecting);
    }

    void failConnecting(std::error_code ec)
    {
        setState(State::failed);
    }

    void connect(Transporting::Ptr transport, AnyBufferCodec codec)
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            setState(State::connecting);
        assert(state() == State::connecting);
        transport_ = std::move(transport);
        codec_ = std::move(codec);
        setState(State::closed);
        onConnect();
    }

    bool establishSession()
    {
        if (!compareAndSetState(State::closed, State::establishing))
            return false;
        assert(transport_ != nullptr);
        onEstablish();
        return true;
    }

    void welcome(SessionId sid, Object opts = {})
    {
        assert(state() == State::authenticating);
        send(Welcome{{}, sid, std::move(opts)});
        setState(State::established);
    }

    bool startShuttingDown()
    {
        return compareAndSetState(State::established, State::shuttingDown);
    }

    void close() {setState(State::closed);}

    void disconnect()
    {
        setState(State::disconnected);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    void fail()
    {
        setState(State::failed);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    virtual ErrorOrDone send(Error&&) = 0;

    virtual ErrorOrDone send(Reason&&) = 0;
    virtual ErrorOrDone send(Realm&&) = 0;
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

    virtual ErrorOrDone abort(Reason r) = 0;

protected:
    explicit Peer(bool isRouter)
        : state_(State::disconnected),
          isRouter_(isRouter)
    {}

    virtual void onConnect() {}

    virtual void onEstablish() {}

    State setState(State s) {return state_.exchange(s);}

    bool compareAndSetState(State expected, State desired)
    {
        return state_.compare_exchange_strong(expected, desired);
    }

    void encode(const Variant& variant, BufferSink sink)
    {
        return codec_.encode(variant, sink);
    }

    std::error_code decode(BufferSource source, Variant& variant)
    {
        return codec_.decode(source, variant);
    }

    Transporting& transport() {return *transport_;}

    PeerListener& listener() {return *listener_;}

    bool isRouter() const {return isRouter_;}

private:
    AnyBufferCodec codec_;
    Transporting::Ptr transport_;
    PeerListener* listener_ = nullptr;
    std::atomic<State> state_;
    bool isRouter_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_PEER_HPP
