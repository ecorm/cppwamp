/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_PEER_HPP
#define CPPWAMP_INTERNAL_PEER_HPP

#include <atomic>
#include <cassert>
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

namespace wamp
{

namespace internal
{

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

    void failConnecting(std::error_code ec)
    {
        setState(State::failed);
        listener_.onPeerFailure(ec, false);
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

    void close() {setState(State::closed);}

    void disconnect()
    {
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
        setState(State::disconnected);
    }

    template <typename C>
    ErrorOrDone send(C&& command) {return sendCommand(command);}

    ErrorOrDone sendError(Error&& error)
    {
        auto done = sendCommand(error);
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            error.withArgs(String{"(snipped)"});
            error.withKwargs({});
            sendCommand(error);
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

        traceTx(msg);
        transport_->sendNowAndClose(std::move(buffer));
        setState(State::failed);
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

    State setState(State s) {return state_.exchange(s);}

    bool compareAndSetState(State expected, State desired)
    {
        return state_.compare_exchange_strong(expected, desired);
    }

    template <typename C>
    ErrorOrDone sendCommand(const C& command)
    {
        const auto& msg = command.message({});
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
        case MessageKind::hello:        return onHello(msg);
        case MessageKind::welcome:      return onWelcome(msg);
        case MessageKind::abort:        return onAbort(msg);
        case MessageKind::challenge:    return onChallenge(msg);
        case MessageKind::authenticate: return onAuthenticate(msg);
        case MessageKind::goodbye:      return onGoodbye(msg);
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
        listener_.onPeerHello(Realm{{}, std::move(msg)});
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
        setState(wasJoining ? State::closed : State::failed);
        listener_.onPeerAbort(std::move(r), wasJoining);
    }

    void onChallenge(Message& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        listener_.onPeerChallenge(Challenge{{}, std::move(msg)});
    }

    void onAuthenticate(Message& msg)
    {
        assert(state() == State::authenticating);
        listener_.onPeerAuthenticate(Authentication{{}, std::move(msg)});
    }

    void onGoodbye(Message& msg)
    {
        Reason reason{{}, std::move(msg)};
        bool isShuttingDown = state() == State::shuttingDown;
        if (isShuttingDown)
        {
            listener_.onPeerGoodbye(std::move(reason), isShuttingDown);
            // Client::leave::Requested will call Peer::close which
            // will set the state to closed.
        }
        else
        {
            setState(State::closed);
            Reason goodbye{WampErrc::goodbyeAndOut};
            send(goodbye);
            listener_.onPeerGoodbye(std::move(reason), isShuttingDown);
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

        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
        setState(State::disconnected);
        listener_.onPeerDisconnect();
    }

    void fail(std::string why, std::error_code ec)
    {
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
        setState(State::failed);
        listener_.onPeerFailure(ec, false, std::move(why));
    }

    void failProtocol(std::string why)
    {
        auto ec = make_error_code(WampErrc::protocolViolation);
        if (readyToAbort())
        {
            auto reason = Reason(ec).withHint(why);
            listener_.onPeerFailure(ec, true, std::move(why));
            abort(std::move(reason));
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

        listener_.onPeerTrace(oss.str());
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
