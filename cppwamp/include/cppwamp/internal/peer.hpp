/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DIALOGUE_HPP
#define CPPWAMP_INTERNAL_DIALOGUE_HPP

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
#include "../logging.hpp"
#include "../sessioninfo.hpp"
#include "../transport.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "wampmessage.hpp"

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
    using Message               = WampMessage;
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

    void log(LogLevel severity, std::string message, std::error_code ec = {})
    {
        if (logLevel() <= severity)
            logHandler_(LogEntry{severity, std::move(message), ec});
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

    void challenge(Challenge challenge)
    {
        assert(isRouter_);
        assert(state() == State::authenticating);
        send(challenge.message({}));
    }

    void welcome(SessionId sid, Object opts = {})
    {
        assert(isRouter_);
        assert(state() == State::authenticating);
        WelcomeMessage msg{sid, std::move(opts)};
        send(msg);
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

    ErrorOrDone send(Message& msg)
    {
        assert(msg.type() != WampMsgType::none);

        MessageBuffer buffer;
        codec_.encode(msg.fields(), buffer);
        if (buffer.size() > maxTxLength_)
            return makeUnexpectedError(WampErrc::payloadSizeExceeded);

        traceTx(msg);
        assert(transport_ != nullptr);
        transport_->send(std::move(buffer));
        return true;
    }

    ErrorOrDone sendError(WampMsgType reqType, RequestId reqId, Error&& error)
    {
        auto done = send(error.errorMessage({}, reqType, reqId));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            error.withArgs(std::string("(Details removed due "
                                       "to transport limits)"));
            error.withKwargs({});
            (void)send(error.errorMessage({}, reqType, reqId));
            if (logLevel() <= LogLevel::warning)
            {
                std::ostringstream oss;
                oss << "Stripped args of outbound ERROR message with error URI "
                    << error.uri() << " and request ID " << reqId
                    << " due to transport payload limits";
                log(LogLevel::warning, oss.str());
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

        auto& msg = r.abortMessage({});
        MessageBuffer buffer;
        codec_.encode(msg.fields(), buffer);

        bool fits = buffer.size() <= maxTxLength_;
        if (!fits)
        {
            msg.options().clear();
            buffer.clear();
            codec_.encode(msg.fields(), buffer);
            log(LogLevel::warning,
                "Stripped options of outbound ABORT message with reason URI " +
                    msg.uri() + ", due to transport payload limits");
        }

        setState(State::failed, r.errorCode());
        traceTx(msg);
        transport_->sendNowAndClose(std::move(buffer));
        if (!fits)
            return makeUnexpectedError(WampErrc::payloadSizeExceeded);
        return true;
    }

private:
    static constexpr unsigned progressiveResponseFlag_ = 0x01;

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
        switch (msg.type())
        {
        case WampMsgType::hello:     return onHello(std::move(msg));
        case WampMsgType::welcome:   return onWelcome(std::move(msg));
        case WampMsgType::abort:     return onAbort(std::move(msg));
        case WampMsgType::challenge: return onChallenge(std::move(msg));
        case WampMsgType::goodbye:   return onGoodbye(std::move(msg));
        default: break;
        }

        // Discard new requests if we're shutting down.
        if (state() == State::shuttingDown && !msg.isReply())
        {
            log(LogLevel::warning,
                "Discarding received " + std::string(msg.name()) +
                    " message while WAMP session is shutting down");
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
        const auto& abortMsg = messageCast<AbortMessage>(msg);
        WampErrc errc = errorUriToCode(abortMsg.uri());

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
                    << abortMsg.uri();
                if (!abortMsg.options().empty())
                    oss << " and details " << abortMsg.options();
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
            const auto& goodbyeMsg = messageCast<GoodbyeMessage>(msg);
            WampErrc errc = errorUriToCode(goodbyeMsg.uri());
            errc = (errc == WampErrc::success) ? WampErrc::closeRealm : errc;

            if (isRouter_)
            {
                inboundMessageHandler_(std::move(msg));
            }
            else if (logLevel() <= LogLevel::warning)
            {
                std::ostringstream oss;
                oss << "Session killed by peer with reason URI "
                    << goodbyeMsg.uri();
                if (!goodbyeMsg.options().empty())
                    oss << " and details " << goodbyeMsg.options();
                log(LogLevel::warning, oss.str());
            }

            setState(State::closed, errc);
            GoodbyeMessage outgoingGoodbye(
                errorCodeToUri(WampErrc::goodbyeAndOut));
            send(outgoingGoodbye).value();
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
            log(LogLevel::warning, "Transport disconnected by remote peer");
    }

    void fail(std::error_code ec, std::string info = {})
    {
        auto oldState = setState(State::failed, ec);
        if (oldState != State::failed)
        {
            if (transport_)
            {
                transport_->close();
                transport_.reset();
            }

            if (!info.empty())
                log(LogLevel::critical, std::move(info), ec);
        }
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

    AnyBufferCodec codec_;
    Transporting::Ptr transport_;
    InboundMessageHandler inboundMessageHandler_;
    LogHandler logHandler_;
    StateChangeHandler stateChangeHandler_;
    std::atomic<State> state_;
    std::atomic<LogLevel> logLevel_;
    std::size_t maxTxLength_ = 0;
    bool isRouter_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIALOGUE_HPP
