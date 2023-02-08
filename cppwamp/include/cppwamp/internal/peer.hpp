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
#include <utility>
#include <boost/asio/strand.hpp>
#include "../anyhandler.hpp"
#include "../codec.hpp"
#include "../erroror.hpp"
#include "../logging.hpp"
#include "../peerdata.hpp"
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
    using OneShotHandler        = AnyCompletionHandler<void (ErrorOr<Message>)>;
    using MultiShotHandler      = std::function<void (ErrorOr<Message>)>;
    using LogHandler            = AnyReusableHandler<void (LogEntry)>;
    using StateChangeHandler    = AnyReusableHandler<void (State,
                                                           std::error_code)>;

    explicit Peer(bool isRouter, AnyIoExecutor exec,
                  AnyCompletionExecutor userExecutor)
        : executor_(std::move(exec)),
          strand_(boost::asio::make_strand(executor_)),
          userExecutor_(std::move(userExecutor)),
          state_(State::disconnected),
          logLevel_(LogLevel::warning),
          isTerminating_(false),
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

    bool isTerminating() const {return isTerminating_.load();}

    const IoStrand& strand() const {return strand_;}

    const AnyIoExecutor& executor() const {return executor_;}

    const AnyCompletionExecutor& userExecutor() const {return userExecutor_;}

    void setInboundMessageHandler(InboundMessageHandler f)
    {
        inboundMessageHandler_ = std::move(f);
    }

    void setLogHandler(LogHandler handler) {logHandler_ = std::move(handler);}

    void setLogLevel(LogLevel level) {logLevel_ = level;}

    LogLevel logLevel() const
    {
        return logHandler_ ? logLevel_.load() : LogLevel::off;
    }

    void log(LogLevel severity, std::string message, std::error_code ec = {})
    {
        if (logHandler_ && (logLevel_.load() <= severity))
        {
            LogEntry entry{severity, std::move(message), ec};
            if (!isTerminating_)
            {
                dispatchVia(executor_, userExecutor_, logHandler_,
                            std::move(entry));
            }
        }
    }

    void setStateChangeHandler(StateChangeHandler handler)
    {
        stateChangeHandler_ = std::move(handler);
    }

    void startConnecting()
    {
        isTerminating_.store(false);
        setState(State::connecting);
    }

    void failConnecting(std::error_code ec) {setState(State::failed, ec);}

    void connect(Transporting::Ptr transport, AnyBufferCodec codec)
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            startConnecting();
        assert(state() == State::connecting);
        transport_ = std::move(transport);
        codec_ = std::move(codec);
        setState(State::closed);
        maxTxLength_ = transport_->info().maxTxLength;
    }

    void establishSession()
    {
        assert(state() == State::closed || state() == State::disconnected);
        assert(transport_ != nullptr);

        setState(State::establishing);

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

    void closeSession(Reason reason, OneShotHandler&& handler)
    {
        // TODO: Reply timeout

        struct Requested
        {
            Peer* self; // Client will keep Peer alive during request
            OneShotHandler handler;

            void operator()(ErrorOr<Message> reply)
            {
                if (reply)
                {
                    self->setState(State::closed);
                    self->abortPending(SessionErrc::sessionEnded);
                }
                handler(std::move(reply));
            }
        };

        assert(state() == State::established);
        setState(State::shuttingDown);
        request(reason.message({}), Requested{this, std::move(handler)});
    }

    void disconnect()
    {
        setState(State::disconnected);
        abortPending(SessionErrc::sessionEnded);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    void terminate()
    {
        isTerminating_.store(true);
        disconnect();
    }

    ErrorOrDone send(Message& msg)
    {
        auto reqId = sendMessage(msg);
        if (!reqId)
            return UnexpectedError(reqId.error());
        return true;
    }

    ErrorOrDone sendError(WampMsgType reqType, RequestId reqId, Error&& error)
    {
        auto done = send(error.errorMessage({}, reqType, reqId));
        if (done == makeUnexpectedError(SessionErrc::payloadSizeExceeded))
        {
            error.withArgs(std::string("(Details removed due "
                                       "to transport limits)"));
            error.withKwargs({});
            (void)send(error.errorMessage({}, reqType, reqId));
            if (logLevel() <= LogLevel::warning)
            {
                std::ostringstream oss;
                oss << "Stripped args of outbound ERROR message due to "
                       "transport payload limits, with error URI "
                    << error.reason() << " and request ID " << reqId;
                log(LogLevel::warning, oss.str());
            }
        }
        return done;
    }

    ErrorOrDone abort(Abort a)
    {
        if (!transport_ || !transport_->isStarted())
            return makeUnexpectedError(SessionErrc::invalidState);

        auto& msg = a.message({});
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
                    msg.reasonUri() + ", due to transport payload limits");
        }

        SessionErrc errc;
        errorUriToCode(a.reason(), SessionErrc::sessionAborted, errc);
        setState(State::failed, make_error_code(errc));
        traceTx(msg);
        transport_->sendNowAndClose(std::move(buffer));
        if (!fits)
            return makeUnexpectedError(SessionErrc::payloadSizeExceeded);
        return true;
    }

    RequestId request(Message& msg, OneShotHandler&& handler)
    {
        // TODO: Move request logic to Client
        return sendRequest(msg, oneShotRequestMap_, std::move(handler));
    }

    RequestId ongoingRequest(Message& msg, MultiShotHandler&& handler)
    {
        return sendRequest(msg, multiShotRequestMap_, std::move(handler));
    }

    ErrorOrDone cancelCall(CallCancellation&& cancellation)
    {
        // If the cancel mode is not 'kill', don't wait for the router's
        // ERROR message and post the request handler immediately
        // with a SessionErrc::cancelled error code.

        bool found = false;
        RequestKey key{WampMsgType::call, cancellation.requestId()};
        auto unex = makeUnexpectedError(SessionErrc::cancelled);

        auto kv = oneShotRequestMap_.find(key);
        if (kv != oneShotRequestMap_.end())
        {
            found = true;
            if (cancellation.mode() != CallCancelMode::kill)
            {
                auto handler = std::move(kv->second);
                oneShotRequestMap_.erase(kv);
                complete(handler, unex);
            }
        }
        else
        {
            auto kv = multiShotRequestMap_.find(key);
            if (kv != multiShotRequestMap_.end())
            {
                found = true;
                if (cancellation.mode() != CallCancelMode::kill)
                {
                    auto handler = std::move(kv->second);
                    multiShotRequestMap_.erase(kv);
                    complete(handler, unex);
                }
            }
        }

        // Always send the CANCEL message in all modes if a matching
        // call was found.
        if (found)
        {
            auto reqId = sendMessage(cancellation.message({}));
            if (!reqId)
                return UnexpectedError(reqId.error());
        }
        return found;
    }

private:
    static constexpr unsigned progressiveResponseFlag_ = 0x01;

    using RequestKey = typename Message::RequestKey;
    using OneShotRequestMap = std::map<RequestKey, OneShotHandler>;
    using MultiShotRequestMap = std::map<RequestKey, MultiShotHandler>;

    template <typename F, typename... Ts>
    void complete(F& handler, Ts&&... args)
    {
        boost::asio::post(strand_, std::bind(std::move(handler),
                                             std::forward<Ts>(args)...));
    }

    State setState(State s, std::error_code ec = {})
    {
        auto old = state_.exchange(s);
        if (old != s && stateChangeHandler_ && !isTerminating_)
            postVia(executor_, userExecutor_, stateChangeHandler_, s, ec);
        return old;
    }

    ErrorOr<RequestId> sendMessage(Message& msg)
    {
        assert(msg.type() != WampMsgType::none);

        auto requestId = setMessageRequestId(msg);

        MessageBuffer buffer;
        codec_.encode(msg.fields(), buffer);
        if (buffer.size() > maxTxLength_)
            return makeUnexpectedError(SessionErrc::payloadSizeExceeded);

        traceTx(msg);
        assert(transport_ != nullptr);
        transport_->send(std::move(buffer));
        return requestId;
    }

    template <typename TRequestMap, typename THandler>
    RequestId sendRequest(Message& msg, TRequestMap& requests,
                          THandler&& handler)
    {
        assert(msg.type() != WampMsgType::none);

        auto requestId = setMessageRequestId(msg);
        MessageBuffer buffer;
        codec_.encode(msg.fields(), buffer);

        if (buffer.size() > maxTxLength_)
        {
            complete(handler,
                     makeUnexpectedError(SessionErrc::payloadSizeExceeded));
            return requestId;
        }

        // In the unlikely (impossible?) event that there is an old pending
        // request with the same request ID (that is, we used the entire
        // 2^53 set of IDs), then cancel the old request.
        auto key = msg.requestKey();
        auto found = requests.find(key);
        if (found != requests.end())
        {
            complete(found->second,
                     makeUnexpectedError(SessionErrc::cancelled));
            requests.erase(found);
        }

        requests.emplace(msg.requestKey(), std::move(handler));
        traceTx(msg);
        assert(transport_ != nullptr);
        transport_->send(std::move(buffer));
        return requestId;
    }

    RequestId setMessageRequestId(Message& msg)
    {
        RequestId requestId = nullId();

        if (msg.hasRequestId())
        {
            requestId = nextRequestId();
            msg.setRequestId(requestId);
        }

        return requestId;
    }

    RequestId nextRequestId()
    {
        // Apply bit mask to constrain the sequence to consecutive integers
        // that can be represented by a double.
        static constexpr auto digits = std::numeric_limits<Real>::digits;
        static constexpr RequestId mask = (1ull << digits) - 1u;
        RequestId n = nextRequestId_ + 1;
        nextRequestId_ = n & mask;
        return nextRequestId_;
    }

    void onTransportRx(MessageBuffer buffer)
    {
        static constexpr auto errc = SessionErrc::protocolViolation;

        auto s = state();
        bool readyToReceive =
               s == State::establishing || s == State::authenticating ||
               s == State::established  || s == State::shuttingDown;
        if (!readyToReceive)
        {
            return fail(errc, "Received WAMP message while session "
                              "unready to receive");
        }

        Variant v;
        auto ec = codec_.decode(buffer, v);
        if (ec)
            return fail(ec, "Error deserializing received WAMP message");

        if (!v.is<Array>())
            return fail(errc, "Received WAMP message is not an array");

        auto& fields = v.as<Array>();
        traceRx(fields);

        auto msg = Message::parse(std::move(fields));
        if (!msg)
        {
            return fail(errc, "Received WAMP message has invalid type number "
                              "or field schema");
        }

        if (!msg->traits().isValidRx(state(), isRouter_))
            return fail(errc, "Received invalid WAMP message for peer role");

        processMessage(std::move(*msg));
    }

    void processMessage(Message&& msg)
    {
        using std::move;

        switch (msg.type())
        {
            case WampMsgType::hello:     return processHello(move(msg));
            case WampMsgType::welcome:   return processWelcome(move(msg));
            case WampMsgType::abort:     return processAbort(move(msg));
            case WampMsgType::challenge: return processChallenge(move(msg));
            case WampMsgType::goodbye:   return processGoodbye(move(msg));
            case WampMsgType::error:     return processWampReply(move(msg));

            default:
                if (msg.repliesTo() == WampMsgType::none)
                {
                    // Role-specific non-reply messages. Ignore it if we're
                    // shutting down.
                    if (state() == State::shuttingDown)
                    {
                        log(LogLevel::warning,
                            "Discarding received " + std::string(msg.name()) +
                            " message while WAMP session is shutting down");
                    }
                    else
                    {
                        inboundMessageHandler_(move(msg));
                    }
                }
                else
                {
                    processWampReply(move(msg));
                }
                break;
        }
    }

    void processWampReply(Message&& msg)
    {
        if (isRouter_)
            return inboundMessageHandler_(std::move(msg));

        bool matchingRequestFound = false;
        auto key = msg.requestKey();
        auto kv = oneShotRequestMap_.find(key);
        if (kv != oneShotRequestMap_.end())
        {
            matchingRequestFound = true;
            auto handler = std::move(kv->second);
            oneShotRequestMap_.erase(kv);
            handler(std::move(msg));
        }
        else
        {
            auto kv = multiShotRequestMap_.find(key);
            if (kv != multiShotRequestMap_.end())
            {
                matchingRequestFound = true;
                if (msg.isProgressiveResponse())
                {
                    const auto& handler = kv->second;
                    handler(std::move(msg));
                }
                else
                {
                    auto handler = std::move(kv->second);
                    multiShotRequestMap_.erase(kv);
                    handler(std::move(msg));
                }
            }
        }

        if (!matchingRequestFound)
        {
            log(LogLevel::warning,
                "Discarding received " + std::string(msg.name()) +
                " message with no matching request");
        }
    }

    void processHello(Message&& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        inboundMessageHandler_(std::move(msg));
    }

    void processWelcome(Message&& msg)
    {
        auto s = state();
        assert(s == State::establishing || s == State::authenticating);
        setState(State::established);
        processWampReply(std::move(msg));
    }

    void processAbort(Message&& msg)
    {
        auto s = state();
        if (s == State::establishing || s == State::authenticating)
        {
            setState(State::closed);
            processWampReply(std::move(msg));
        }
        else if (s == State::shuttingDown)
        {
            log(LogLevel::warning, "Discarding received ABORT message "
                                   "while session is shutting down");
        }
        else
        {
            const auto& abortMsg = messageCast<AbortMessage>(msg);
            SessionErrc errc = {};
            errorUriToCode(abortMsg.reasonUri(),
                           SessionErrc::sessionAbortedByPeer, errc);

            if (logLevel() <= LogLevel::error)
            {
                std::ostringstream oss;
                oss << "Session aborted by peer with reason URI "
                    << abortMsg.reasonUri();
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

    void processChallenge(Message&& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        inboundMessageHandler_(std::move(msg));
    }

    void processGoodbye(Message&& msg)
    {
        if (state() == State::shuttingDown)
        {
            setState(State::closed);
            processWampReply(std::move(msg));
        }
        else
        {
            const auto& goodbyeMsg = messageCast<GoodbyeMessage>(msg);
            SessionErrc errc;
            errorUriToCode(goodbyeMsg.reasonUri(), SessionErrc::closeRealm,
                           errc);

            if (isRouter_)
            {
                inboundMessageHandler_(std::move(msg));
            }
            else if (logLevel() <= LogLevel::warning)
            {
                std::ostringstream oss;
                oss << "Session ended by peer with reason URI "
                    << goodbyeMsg.reasonUri();
                if (!goodbyeMsg.options().empty())
                    oss << " and details " << goodbyeMsg.options();
                log(LogLevel::warning, oss.str());
            }

            abortPending(errc);
            setState(State::closed);
            GoodbyeMessage outgoingGoodbye("wamp.error.goodbye_and_out");
            send(outgoingGoodbye).value();
        }
    }

    void onRemoteDisconnect()
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            return;

        setState(State::disconnected);
        abortPending(make_error_code(TransportErrc::disconnected));
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
            abortPending(ec);
            if (transport_)
            {
                transport_->close();
                transport_.reset();
            }

            if (!info.empty())
                log(LogLevel::error, std::move(info), ec);
        }
    }

    template <typename TErrc>
    void fail(TErrc errc, std::string info = {})
    {
        fail(make_error_code(errc), std::move(info));
    }

    void abortPending(std::error_code ec)
    {
        UnexpectedError unex{ec};
        if (!isTerminating_)
        {
            for (auto& kv: oneShotRequestMap_)
                complete(kv.second, unex);
            for (auto& kv: multiShotRequestMap_)
                complete(kv.second, unex);
        }
        oneShotRequestMap_.clear();
        multiShotRequestMap_.clear();
    }

    template <typename TErrc>
    void abortPending(TErrc errc) {abortPending(make_error_code(errc));}

    void traceRx(const Array& fields)
    {
        trace(Message::parseMsgType(fields), fields, "Rx message: ");
    }

    void traceTx(const Message& msg)
    {
        trace(msg.type(), msg.fields(), "Tx message: ");
    }

    void trace(WampMsgType type, const Array& fields, const char* label)
    {
        if (isTerminating_ || (logLevel() > LogLevel::trace))
            return;

        std::ostringstream oss;
        oss << label << "[";
        if (!fields.empty())
        {
            // Print message type field as {"NAME":<Field>} pair
            auto name = MessageTraits::lookup(type).nameOr("INVALID");
            oss << "{\"" << name << "\":" << fields[0] << "}";

            for (Array::size_type i=1; i<fields.size(); ++i)
                oss << "," << fields[i];
        }
        oss << ']';

        LogEntry entry{LogLevel::trace, oss.str()};
        dispatchVia(strand_, userExecutor_, logHandler_, std::move(entry));
    }

    AnyIoExecutor executor_;
    IoStrand strand_;
    AnyCompletionExecutor userExecutor_;
    AnyBufferCodec codec_;
    Transporting::Ptr transport_;
    InboundMessageHandler inboundMessageHandler_;
    LogHandler logHandler_;
    StateChangeHandler stateChangeHandler_;
    OneShotRequestMap oneShotRequestMap_;
    MultiShotRequestMap multiShotRequestMap_;
    std::atomic<State> state_;
    std::atomic<LogLevel> logLevel_;
    std::atomic<bool> isTerminating_;
    RequestId nextRequestId_ = nullId();
    std::size_t maxTxLength_ = 0;
    bool isRouter_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIALOGUE_HPP
