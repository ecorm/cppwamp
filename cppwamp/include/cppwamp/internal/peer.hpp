/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_PEER_HPP
#define CPPWAMP_INTERNAL_PEER_HPP

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
#include "../pubsubinfo.hpp"
#include "../rpcinfo.hpp"
#include "../sessioninfo.hpp"
#include "../transport.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "commandinfo.hpp"
#include "message.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class PeerListener
{
public:
    virtual void onStateChanged(SessionState, std::error_code) = 0;

    virtual void onFailure(std::string&& why, std::error_code ec,
                           bool abortSent) = 0;

    virtual void onTrace(std::string&& messageDump) = 0;

    virtual void onPeerHello(Realm&&) {assert(false);}

    virtual void onPeerWelcome(Welcome&& w)
    {
        onPeerMessage(std::move(w.message({})));
    }

    virtual void onPeerAbort(Reason&&, bool wasJoining) = 0;

    virtual void onPeerChallenge(Challenge&& c) {assert(false);}

    virtual void onPeerAuthenticate(Challenge&& c) {assert(false);}

    virtual void onPeerGoodbye(Reason&&) = 0;

    virtual void onPeerMessage(Message&& m)
    {
        using K = MessageKind;
        switch (m.kind())
        {
        case K::error:          return onPeerCommand(Error{{},            std::move(m)});
        case K::publish:        return onPeerCommand(Pub{{},              std::move(m)});
        case K::published:      return onPeerCommand(Published{{},        std::move(m)});
        case K::subscribe:      return onPeerCommand(Topic{{},            std::move(m)});
        case K::subscribed:     return onPeerCommand(Subscribed{{},       std::move(m)});
        case K::unsubscribe:    return onPeerCommand(Unsubscribe{{},      std::move(m)});
        case K::unsubscribed:   return onPeerCommand(Unsubscribed{{},     std::move(m)});
        case K::event:          return onPeerCommand(Event{{},            std::move(m)});
        case K::call:           return onPeerCommand(Rpc{{},              std::move(m)});
        case K::cancel:         return onPeerCommand(CallCancellation{{}, std::move(m)});
        case K::result:         return onPeerCommand(Result{{},           std::move(m)});
        case K::enroll:         return onPeerCommand(Procedure{{},        std::move(m)});
        case K::registered:     return onPeerCommand(Registered{{},       std::move(m)});
        case K::unregister:     return onPeerCommand(Unregister{{},       std::move(m)});
        case K::unregistered:   return onPeerCommand(Unregistered{{},     std::move(m)});
        case K::invocation:     return onPeerCommand(Invocation{{},       std::move(m)});
        case K::interrupt:      return onPeerCommand(Interruption{{},     std::move(m)});
        case K::yield:          return onPeerCommand(Result{{},           std::move(m)});
        default: assert(false && "Unexpected MessageKind enumerator");
        }
    }

    virtual void onPeerCommand(Authentication&& c)   {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Error&& c)            {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Pub&& c)              {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Published&& c)        {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Topic&& c)            {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Subscribed&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unsubscribe&& c)      {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unsubscribed&& c)     {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Event&& c)            {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Rpc&& c)              {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(CallCancellation&& c) {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Result&& c)           {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Procedure&& c)        {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Registered&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unregister&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unregistered&& c)     {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Invocation&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Interruption&& c)     {onPeerMessage(std::move(c.message({})));}

    void enableTrace(bool enabled = true) {traceEnabled_.store(enabled);}

    bool traceEnabled() const {return traceEnabled_.load();}

protected:
    PeerListener() : traceEnabled_(false) {}

private:
    std::atomic<bool> traceEnabled_;
};


//------------------------------------------------------------------------------
// Provides session functionality common to both clients and router peers.
//------------------------------------------------------------------------------
class Peer
{
public:
    using State = SessionState;

    explicit Peer(PeerListener* listener, bool isRouter)
        : listener_(*listener),
          state_(State::disconnected),
          isRouter_(isRouter)
    {}

    virtual ~Peer()
    {
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    State state() const {return state_.load();}

    bool startConnecting()
    {
        return compareAndSetState(State::disconnected, State::connecting);
    }

    void failConnecting(std::error_code ec) {setState(State::failed, ec);}

    void connect(Transporting::Ptr transport, AnyBufferCodec codec)
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            setState(State::connecting);
        assert(state() == State::connecting);
        transport_ = std::move(transport);
        codec_ = std::move(codec);
        setState(State::closed);
        maxTxLength_ = transport_->info().maxTxLength;
    }

    bool establishSession()
    {
        if (!compareAndSetState(State::closed, State::establishing))
            return false;
        assert(transport_ != nullptr);

        if (!transport_->isStarted())
        {
            transport_->start(
                [this](ErrorOr<MessageBuffer> buffer)
                {
                    // Ignore transport cancellation errors when disconnecting.
                    if (buffer.has_value())
                        onTransportRx(std::move(*buffer));
                    else if (buffer.error() == TransportErrc::disconnected)
                        onRemoteDisconnect();
                    else if (state() != State::disconnected)
                        fail("Transport receive failure", buffer.error());
                },
                [this](std::error_code ec)
                {
                    // Ignore transport cancellation errors when disconnecting.
                    if (state() != State::disconnected)
                        fail("Transport send failure", ec);
                }
            );
        }

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

    void close()
    {
        setState(State::closed);
    }

    void disconnect()
    {
        setState(State::disconnected);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    template <typename TCommand>
    ErrorOrDone send(TCommand&& cmd)
    {
        return doSend(cmd);
    }

    ErrorOrDone sendError(Error&& error)
    {
        auto done = doSend(error);
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            error.withArgs(std::string("(snipped)"));
            error.withKwargs({});
            doSend(error);
        }
        return done;
    }

    ErrorOrDone abort(Reason r)
    {
        if (!transport_ || !transport_->isStarted())
            return makeUnexpectedError(Errc::invalidState);

        if (!readyToAbort())
        {
            disconnect();
            return makeUnexpectedError(Errc::invalidState);
        }

        r.setKindToAbort({});
        const auto& msg = r.message({});
        MessageBuffer buffer;
        codec_.encode(msg.fields(), buffer);

        bool fits = buffer.size() <= maxTxLength_;
        if (!fits)
        {
            r.options().clear();
            r.withHint("(snipped)");
            buffer.clear();
            codec_.encode(msg.fields(), buffer);
        }

        setState(State::failed, r.errorCode());
        traceTx(msg);
        transport_->sendNowAndClose(std::move(buffer));
        if (!fits)
            return makeUnexpectedError(WampErrc::payloadSizeExceeded);
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

    template <typename TCommand>
    ErrorOrDone doSend(const TCommand& cmd)
    {
        const auto& msg = cmd.message({});
        assert(msg.kind() != MessageKind::none);

        MessageBuffer buffer;
        codec_.encode(msg.fields(), buffer);
        if (buffer.size() > maxTxLength_)
            return makeUnexpectedError(WampErrc::payloadSizeExceeded);

        traceTx(msg);
        assert(transport_ != nullptr);
        transport_->send(std::move(buffer));
        return true;
    }

    void onTransportRx(MessageBuffer buffer)
    {
        Variant v;
        auto ec = codec_.decode(buffer, v);
        if (ec)
            return failProtocol("Error deserializing received WAMP message: " +
                                detailedErrorCodeString(ec));

        if (!v.is<Array>())
            return failProtocol("Received WAMP message is not an array");

        auto& fields = v.as<Array>();
        traceRx(fields);

        auto msg = Message::parse(std::move(fields));
        if (!msg)
        {
            return failProtocol("Received WAMP message has invalid type number "
                                "or field schema");
        }

        bool isValidForRole = isRouter_ ? msg->traits().isRouterRx
                                        : msg->traits().isClientRx;
        if (!isValidForRole)
        {
            return failProtocol("Role does not support receiving " +
                                std::string(msg->name()) + " messages");
        }

        auto s = state();
        if (!msg->traits().isValidForState(s))
        {
            return failProtocol(
                std::string(msg->name()) + " messages are invalid during " +
                stateLabel(s) + " session state");
        }

        onMessage(*msg);
    }

    void onMessage(Message& msg)
    {
        switch (msg.kind())
        {
        case MessageKind::hello:     return onHello(msg);
        case MessageKind::welcome:   return onWelcome(msg);
        case MessageKind::abort:     return onAbort(msg);
        case MessageKind::challenge: return onChallenge(msg);
        case MessageKind::goodbye:   return onGoodbye(msg);
        default: break;
        }

        // Discard new requests if we're shutting down.
        if (state() == State::shuttingDown && !msg.isReply())
            return;

        notifyMessage(msg);
    }

    void onHello(Message& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        notifyMessage(msg);
    }

    void onWelcome(Message& msg)
    {
        auto s = state();
        assert(s == State::establishing || s == State::authenticating);
        setState(State::established);
        notifyMessage(msg);
    }

    void onAbort(Message& msg)
    {
        auto s = state();
        bool wasJoining = s == State::establishing ||
                          s == State::authenticating;
        Reason r{{}, std::move(msg)};
        if (wasJoining)
            setState(State::closed);
        else
            setState(State::failed, r.errorCode());
        listener_.onPeerAbort(std::move(r), wasJoining);
    }

    void onChallenge(Message& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        notifyMessage(msg);
    }

    void onGoodbye(Message& msg)
    {
        Reason reason{{}, std::move(msg)};
        if (state() == State::shuttingDown)
        {
            setState(State::closed);
            listener_.onPeerGoodbye(std::move(reason));
        }
        else
        {
            WampErrc errc = reason.errorCode();
            errc = (errc == WampErrc::success) ? WampErrc::closeRealm : errc;
            listener_.onPeerGoodbye(std::move(reason));
            setState(State::closed, errc);
            Reason goodbye{WampErrc::goodbyeAndOut};
            send(goodbye);
        }
    }

    void notifyMessage(Message& msg)
    {
        listener_.onPeerMessage(std::move(msg));
    }

    void onRemoteDisconnect()
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            return;

        setState(State::disconnected, TransportErrc::disconnected);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    void fail(std::string why, std::error_code ec)
    {
        setState(State::failed, ec);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
        listener_.onFailure(std::move(why), ec, false);
    }

    void failProtocol(std::string why)
    {
        auto ec = make_error_code(WampErrc::protocolViolation);
        if (readyToAbort())
        {
            abort(Reason(ec).withHint(why));
            listener_.onFailure(std::move(why), ec, true);
        }
        else
        {
            fail(std::move(why), ec);
        }
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

    void trace(MessageKind kind, const Array& fields, const char* label)
    {
        if (!listener_.traceEnabled())
            return;

        std::ostringstream oss;
        oss << "[\"" << label << "\",\""
            << MessageTraits::lookup(kind).nameOr("INVALID") << "\"";
        if (!fields.empty())
            oss << "," << fields;
        oss << ']';

        listener_.onTrace(oss.str());
    }

    AnyBufferCodec codec_;
    Transporting::Ptr transport_;
    PeerListener& listener_;
    std::atomic<State> state_;
    std::size_t maxTxLength_ = 0;
    bool isRouter_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_PEER_HPP
