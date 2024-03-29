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
    using LogStringHandler      = AnyReusableHandler<void (std::string)>; // TODO: Remove
    using StateChangeHandler    = AnyReusableHandler<void (State)>;

    explicit Peer(bool isRouter, AnyIoExecutor exec)
        : strand_(boost::asio::make_strand(exec)),
          userExecutor_(std::move(exec)),
          state_(State::disconnected),
          logLevel_(LogLevel::warning),
          isTerminating_(false),
          isRouter_(isRouter)
    {}

    explicit Peer(bool isRouter, const AnyIoExecutor& exec,
                  AnyCompletionExecutor userExecutor)
        : Peer(isRouter, boost::asio::make_strand(exec),
               std::move(userExecutor))
    {}

    explicit Peer(bool isRouter, IoStrand strand,
                  AnyCompletionExecutor userExecutor)
        : strand_(std::move(strand)),
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

    const IoStrand& strand() const {return strand_;}

    const AnyCompletionExecutor& userExecutor() const {return userExecutor_;}

    void setInboundMessageHandler(InboundMessageHandler f)
    {
        inboundMessageHandler_ = std::move(f);
    }

    void setLogHandler(LogHandler handler) {logHandler_ = std::move(handler);}

    void setLogLevel(LogLevel level) {logLevel_ = level;}

    LogLevel logLevel() const
    {
        if (logHandler_)
            return logLevel_.load();
        if (warningHandler_)
            return LogLevel::warning;
        if (traceHandler_)
            return LogLevel::trace;
        return LogLevel::off;
    }

    void log(LogLevel severity, std::string message, std::error_code ec = {})
    {
        if (logHandler_ && (logLevel_.load() <= severity))
        {
            LogEntry entry{severity, std::move(message), ec};
            if (!isTerminating_)
                dispatchVia(userExecutor_, logHandler_, std::move(entry));
        }

        if (warningHandler_ && (severity >= LogLevel::warning))
        {
            std::ostringstream oss;
            oss << message;
            if (ec)
            {
                oss << " (with error code " << ec
                    << " '" << ec.message() << "')";
            }
            if (!isTerminating_)
                dispatchVia(userExecutor_, warningHandler_, oss.str());
        }
    }

    // TODO: Remove
    void setWarningHandler(LogStringHandler handler)
    {
        warningHandler_ = std::move(handler);
    }

    // TODO: Remove
    void setTraceHandler(LogStringHandler handler)
    {
        traceHandler_ = std::move(handler);
    }

    void setStateChangeHandler(StateChangeHandler handler)
    {
        stateChangeHandler_ = std::move(handler);
    }

    State setState(State s)
    {
        auto old = state_.exchange(s);
        if (old != s && stateChangeHandler_ && !isTerminating_)
            postVia(userExecutor_, stateChangeHandler_, s);
        return old;
    }

    void setTerminating(bool terminating) {isTerminating_.store(terminating);}

    bool isTerminating() const {return isTerminating_.load();}

    void open(Transporting::Ptr transport, AnyBufferCodec codec)
    {
        assert(state() == State::connecting);
        transport_ = std::move(transport);
        codec_ = std::move(codec);
        setState(State::closed);
        maxTxLength_ = transport_->info().maxTxLength;
    }

    void start()
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
                    else if (state() != SessionState::disconnected)
                        fail(buffer.error(), "Transport receive failure");
                },
                [this](std::error_code ec)
                {
                    // Ignore transport cancellation errors when disconnecting.
                    if (state() != SessionState::disconnected)
                        fail(ec, "Transport send failure");
                }
            );
        }
    }

    void adjourn(Reason& reason, OneShotHandler&& handler)
    {
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

        using std::move;
        assert(state() == State::established);
        setState(State::shuttingDown);
        request(reason.message({}), Requested{this, std::move(handler)});
    }

    void close()
    {
        setState(State::disconnected);
        abortPending(SessionErrc::sessionEnded);
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
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

    RequestId request(Message& msg, OneShotHandler&& handler)
    {
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
                post(std::move(handler), unex);
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
                    post(std::move(handler), unex);
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

    template <typename TFunctor, typename... TArgs>
    void post(TFunctor&& fn, TArgs&&... args)
    {
        boost::asio::post(strand_,
                          std::bind(std::forward<TFunctor>(fn),
                                    std::forward<TArgs>(args)...));
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
            post(std::move(handler),
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
            post(std::move(found->second),
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
        RequestId requestId = nullRequestId();

        if (msg.hasRequestId())
        {
            requestId = nextRequestId();
            msg.setRequestId(requestId);
        }

        return requestId;
    }

    RequestId nextRequestId()
    {
        if (nextRequestId_ >= maxRequestId_)
            nextRequestId_ = nullRequestId();
        return ++nextRequestId_;
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
        if (msg.repliesTo() != WampMsgType::none)
        {
            processWampReply( RequestKey(msg.repliesTo(), msg.requestId()),
                              std::move(msg) );
        }
        else switch(msg.type())
        {
            case WampMsgType::hello:
                processHello(std::move(msg));
                break;

            case WampMsgType::challenge:
                processChallenge(std::move(msg));
                break;

            case WampMsgType::welcome:
                processWelcome(std::move(msg));
                break;

            case WampMsgType::abort:
                processAbort(std::move(msg));
                break;

            case WampMsgType::goodbye:
                processGoodbye(std::move(msg));
                break;

            case WampMsgType::error:
                processWampReply(msg.requestKey(), std::move(msg));
                break;

            default:
                // Role-specific unsolicited messages. Ignore them if we're
                // shutting down.
                if (state() == State::shuttingDown)
                {
                    log(LogLevel::warning,
                        "Discarding received " + std::string(msg.name()) +
                        " message while session is shutting down");
                }
                else
                {
                    inboundMessageHandler_(std::move(msg));
                }
                break;
        }
    }

    void processWampReply(const RequestKey& key, Message&& msg)
    {
        auto kv = oneShotRequestMap_.find(key);
        if (kv != oneShotRequestMap_.end())
        {
            auto handler = std::move(kv->second);
            oneShotRequestMap_.erase(kv);
            handler(std::move(msg));
        }
        else
        {
            auto kv = multiShotRequestMap_.find(key);
            if (kv != multiShotRequestMap_.end())
            {
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
    }

    void processHello(Message&& msg)
    {
        assert(state() == State::establishing);
        setState(State::established);
        inboundMessageHandler_(std::move(msg));
    }

    void processChallenge(Message&& msg)
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
        processWampReply(RequestKey(WampMsgType::hello, 0), std::move(msg));
    }

    void processAbort(Message&& msg)
    {
        auto s = state();
        if (s == State::establishing || s == State::authenticating)
        {
            setState(State::closed);
            processWampReply(RequestKey(WampMsgType::hello, 0), std::move(msg));
        }
        else if (s == State::shuttingDown)
        {
            log(LogLevel::warning, "Discarding received ABORT message "
                                   "while session is shutting down");
        }
        else
        {
            const auto& abortMsg = message_cast<AbortMessage>(msg);
            SessionErrc errc = {};
            lookupWampErrorUri(abortMsg.reasonUri(),
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

    void processGoodbye(Message&& msg)
    {
        if (state() == State::shuttingDown)
        {
            setState(State::closed);
            processWampReply(msg.requestKey(), std::move(msg));
        }
        else
        {
            const auto& goodbyeMsg = message_cast<GoodbyeMessage>(msg);
            SessionErrc errc;
            lookupWampErrorUri(goodbyeMsg.reasonUri(), SessionErrc::closeRealm,
                               errc);
            abortPending(errc);
            GoodbyeMessage outgoingGoodbye("wamp.error.goodbye_and_out");
            send(outgoingGoodbye).value();
            setState(State::closed);

            if (logLevel() <= LogLevel::warning)
            {
                std::ostringstream oss;
                oss << "Session ended by peer with reason URI "
                    << goodbyeMsg.reasonUri();
                if (!goodbyeMsg.options().empty())
                    oss << " and details " << goodbyeMsg.options();
                log(LogLevel::warning, oss.str());
            }
        }
    }

    void fail(std::error_code ec, std::string info = {})
    {
        auto oldState = setState(State::failed);
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
                post(std::move(kv.second), unex);
            for (auto& kv: multiShotRequestMap_)
                post(std::move(kv.second), unex);
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

        if (logHandler_)
        {
            LogEntry entry{LogLevel::trace, oss.str()};
            dispatchVia(userExecutor_, logHandler_, std::move(entry));
        }

        if (traceHandler_)
            dispatchVia(userExecutor_, traceHandler_, oss.str());
    }

    IoStrand strand_;
    AnyCompletionExecutor userExecutor_;
    AnyBufferCodec codec_;
    Transporting::Ptr transport_;
    InboundMessageHandler inboundMessageHandler_;
    LogHandler logHandler_;
    LogStringHandler warningHandler_;
    LogStringHandler traceHandler_;
    StateChangeHandler stateChangeHandler_;
    OneShotRequestMap oneShotRequestMap_;
    MultiShotRequestMap multiShotRequestMap_;
    std::atomic<State> state_;
    std::atomic<LogLevel> logLevel_;
    std::atomic<bool> isTerminating_;
    RequestId nextRequestId_ = nullRequestId();
    std::size_t maxTxLength_ = 0;
    bool isRouter_ = false;

    static constexpr RequestId maxRequestId_ = 9007199254740992ull;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIALOGUE_HPP
