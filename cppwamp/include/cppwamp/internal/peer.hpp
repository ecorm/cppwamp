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
#include "../logging.hpp"
#include "../sessioninfo.hpp"
#include "../transport.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "message.hpp"

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
    using State                 = SessionState;
    using InboundMessageHandler = std::function<void (Message)>;
    using LogHandler            = std::function<void (LogEntry)>;
    using StateChangeHandler    = std::function<void (State, std::error_code)>;

    explicit Peer(bool isRouter)
        : state_(State::disconnected),
          logLevel_(LogLevel::warning),
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
                        fail(buffer.error(), "Transport receive failure");
                },
                [this](std::error_code ec)
                {
                    // Ignore transport cancellation errors when disconnecting.
                    if (state() != State::disconnected)
                        fail(ec, "Transport send failure");
                }
            );
        }

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
            error.withArgs(std::string("(Details removed due "
                                       "to transport limits)"));
            error.withKwargs({});
            doSend(error);
            if (logLevel() <= LogLevel::warning)
            {
                std::ostringstream oss;
                oss << "Stripped args of outbound ERROR message with error URI "
                    << error.uri() << " and request ID " << error.requestId({})
                    << " due to transport payload limits";
                log({LogLevel::warning, oss.str()});
            }
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
            buffer.clear();
            codec_.encode(msg.fields(), buffer);
            log({LogLevel::warning,
                 "Stripped options of outbound ABORT message with reason URI " +
                     r.uri() + ", due to transport payload limits"});
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

        onMessage(std::move(*msg));
    }

    void onMessage(Message&& msg)
    {
        switch (msg.kind())
        {
        case MessageKind::hello:     return onHello(std::move(msg));
        case MessageKind::welcome:   return onWelcome(std::move(msg));
        case MessageKind::abort:     return onAbort(std::move(msg));
        case MessageKind::challenge: return onChallenge(std::move(msg));
        case MessageKind::goodbye:   return onGoodbye(std::move(msg));
        default: break;
        }

        // Discard new requests if we're shutting down.
        if (state() == State::shuttingDown && !msg.isReply())
        {
            log({LogLevel::warning,
                 "Discarding received " + std::string(msg.name()) +
                     " message while WAMP session is shutting down"});
            return;
        }

        inboundMessageHandler_(std::move(msg));
    }

    void onHello(Message&& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        inboundMessageHandler_(std::move(msg));
    }

    void onWelcome(Message&& msg)
    {
        auto s = state();
        assert(s == State::establishing || s == State::authenticating);
        setState(State::established);
        inboundMessageHandler_(std::move(msg));
    }

    void onAbort(Message&& msg)
    {
        auto s = state();
        Reason reason{{}, std::move(msg)};
        WampErrc errc = reason.errorCode();

        if (s == State::establishing || s == State::authenticating)
        {
            setState(State::closed);
            inboundMessageHandler_(std::move(msg));
        }
        else
        {
            if (logLevel() <= LogLevel::critical)
            {
                std::ostringstream oss;
                oss << "Session aborted by peer with reason URI "
                    << reason.uri();
                if (!reason.options().empty())
                    oss << " and details " << reason.options();
                fail(errc, oss.str());
            }
            else
            {
                fail(errc);
            }
        }
    }

    void onChallenge(Message&& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        inboundMessageHandler_(std::move(msg));
    }

    void onGoodbye(Message&& msg)
    {
        if (state() == State::shuttingDown)
        {
            setState(State::closed);
            inboundMessageHandler_(std::move(msg));
        }
        else
        {
            Reason reason{{}, std::move(msg)};
            WampErrc errc = reason.errorCode();
            errc = (errc == WampErrc::success) ? WampErrc::closeRealm : errc;

            if (isRouter_)
            {
                inboundMessageHandler_(std::move(msg));
            }
            else if (logLevel() <= LogLevel::warning)
            {
                std::ostringstream oss;
                oss << "Session killed by peer with reason URI "
                    << reason.uri();
                if (!reason.options().empty())
                    oss << " and details " << reason.options();
                log({LogLevel::warning, oss.str()});
            }

            setState(State::closed, errc);
            Reason goodbye{WampErrc::goodbyeAndOut};
            send(goodbye);
        }
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

        if (!isRouter_)
            log({LogLevel::warning, "Transport disconnected by remote peer"});
    }

    void fail(std::error_code ec, std::string info = {})
    {
        setState(State::failed, ec);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
        if (!info.empty())
            log({LogLevel::critical, std::move(info), ec});
    }

    template <typename TErrc>
    void fail(TErrc errc, std::string info = {})
    {
        fail(make_error_code(errc), std::move(info));
    }

    void failProtocol(std::string info)
    {
        auto errc = WampErrc::protocolViolation;
        if (readyToAbort())
            abort(Reason(errc).withHint(std::move(info)));
        else
            fail(errc, std::move(info));
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
        if (logLevel() > LogLevel::trace)
            return;

        std::ostringstream oss;
        oss << "[\"" << label << "\",\""
            << MessageTraits::lookup(kind).nameOr("INVALID") << "\"";
        if (!fields.empty())
            oss << "," << fields;
        oss << ']';

        LogEntry entry{LogLevel::trace, oss.str()};
        logHandler_(std::move(entry));
    }

    InboundMessageHandler inboundMessageHandler_;
    LogHandler logHandler_;
    StateChangeHandler stateChangeHandler_;
    AnyBufferCodec codec_;
    Transporting::Ptr transport_;
    std::atomic<State> state_;
    std::atomic<LogLevel> logLevel_;
    std::size_t maxTxLength_ = 0;
    bool isRouter_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_PEER_HPP
