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
#include "../peerdata.hpp"
#include "../error.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "wampmessage.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Base class providing session functionality common to both clients and
// router peers. This class is extended by Client to implement a client session.
//------------------------------------------------------------------------------
template <typename TCodec, typename TTransport>
class Peer : public std::enable_shared_from_this<Peer<TCodec, TTransport>>
{
public:
    using Codec        = TCodec;
    using Transport    = TTransport;
    using TransportPtr = std::shared_ptr<Transport>;
    using State        = SessionState;

    State state() const {return state_.load();}

    IoStrand strand() const {return strand_;}

protected:
    using Message = WampMessage;
    using OneShotHandler =
        AnyCompletionHandler<void (std::error_code, Message)>;
    using MultiShotHandler = std::function<void (std::error_code, Message)>;
    using LogHandler = AnyReusableHandler<void (std::string)>;
    using StateChangeHandler = AnyReusableHandler<void (State)>;

    explicit Peer(TransportPtr&& transport)
        : strand_(transport->strand()),
          transport_(std::move(transport)),
          state_(State::closed)
    {}

    virtual bool isMsgSupported(const MessageTraits& traits) = 0;

    virtual void onInbound(Message msg) = 0;

    const AnyIoExecutor& userExecutor() const {return userExecutor_;}

    void start()
    {
        assert(state() == State::closed || state() == State::disconnected);
        setState(State::establishing);

        if (!transport_->isStarted())
        {
            std::weak_ptr<Peer> self(this->shared_from_this());

            transport_->start(
                [self](MessageBuffer buffer)
                {
                    auto me = self.lock();
                    if (me)
                        me->onTransportRx(std::move(buffer));
                },
                [self](std::error_code ec)
                {
                    auto me = self.lock();
                    if (me)
                        me->checkError(ec);
                }
            );
        }
    }

    void adjourn(Reason& reason, OneShotHandler&& handler)
    {
        struct Requested
        {
            std::shared_ptr<Peer> self;
            OneShotHandler handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (!ec)
                {
                    me.setState(State::closed);
                    me.abortPending(make_error_code(SessionErrc::sessionEnded));
                    me.post(std::move(handler),
                            make_error_code(ProtocolErrc::success),
                            std::move(reply));
                }
                else
                    me.post(move(handler), ec, Message());
            }
        };

        using std::move;
        assert(state() == State::established);
        setState(State::shuttingDown);
        auto self = this->shared_from_this();
        request(reason.message({}),
                Requested{this->shared_from_this(), std::move(handler)});
    }

    void close(bool terminating)
    {
        setState(State::disconnected);
        abortPending(make_error_code(SessionErrc::sessionEnded), terminating);
        transport_->close();
    }

    void send(Message& msg)
    {
        sendMessage(msg);
    }

    void sendError(WampMsgType reqType, RequestId reqId, Error&& error)
    {
        send(error.errorMessage({}, reqType, reqId));
    }

    RequestId request(Message& msg, OneShotHandler&& handler)
    {
        return sendRequest(msg, oneShotRequestMap_, std::move(handler));
    }

    RequestId ongoingRequest(Message& msg, MultiShotHandler&& handler)
    {
        return sendRequest(msg, multiShotRequestMap_, std::move(handler));
    }

    void cancelCall(CallCancellation&& cancellation)
    {
        // If the cancel mode is not 'kill', don't wait for the router's
        // ERROR message and post the request handler immediately
        // with a SessionErrc::cancelled error code.
        RequestKey key{WampMsgType::call, cancellation.requestId()};
        if (cancellation.mode() != CallCancelMode::kill)
        {
            auto kv = oneShotRequestMap_.find(key);
            if (kv != oneShotRequestMap_.end())
            {
                auto handler = std::move(kv->second);
                oneShotRequestMap_.erase(kv);
                auto ec = make_error_code(SessionErrc::cancelled);
                post(std::move(handler), ec, Message());
            }
            else
            {
                auto kv = multiShotRequestMap_.find(key);
                if (kv != multiShotRequestMap_.end())
                {
                    auto handler = std::move(kv->second);
                    multiShotRequestMap_.erase(kv);
                    auto ec = make_error_code(SessionErrc::cancelled);
                    post(std::move(handler), ec, Message());
                }
            }
        }

        // Always send the CANCEL message in all modes.
        sendMessage(cancellation.message({}));
    }

    template <typename TErrorValue>
    void fail(TErrorValue errc)
    {
        fail(make_error_code(errc));
    }

    void setUserExecutor(AnyIoExecutor exec)
    {
        userExecutor_ = std::move(exec);
    }

    void setTraceHandler(LogHandler handler)
    {
        traceHandler_ = std::move(handler);
    }

    void setStateChangeHandler(StateChangeHandler handler)
    {
        stateChangeHandler_ = std::move(handler);
    }

    template <typename TFunctor, typename... TArgs>
    void post(TFunctor&& fn, TArgs&&... args)
    {
        boost::asio::post(strand_,
                          std::bind(std::forward<TFunctor>(fn),
                                    std::forward<TArgs>(args)...));
    }

private:
    static constexpr unsigned progressiveResponseFlag_ = 0x01;

    using RequestKey = typename Message::RequestKey;
    using OneShotRequestMap = std::map<RequestKey, OneShotHandler>;
    using MultiShotRequestMap = std::map<RequestKey, MultiShotHandler>;
    using EncoderType = Encoder<Codec, MessageBuffer>;
    using DecoderType = Decoder<Codec, MessageBuffer>;

    void setState(State s)
    {
        auto old = state_.exchange(s);
        if (old != s && stateChangeHandler_)
            postVia(userExecutor_, stateChangeHandler_, s);
    }

    RequestId sendMessage(Message& msg)
    {
        assert(msg.type() != WampMsgType::none);

        auto requestId = setMessageRequestId(msg);

        MessageBuffer buffer;
        encoder_.encode(msg.fields(), buffer);
        if (buffer.size() > transport_->maxSendLength())
            throw error::Failure(make_error_code(TransportErrc::badTxLength));

        trace(msg, true);
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
        encoder_.encode(msg.fields(), buffer);
        if (buffer.size() > transport_->maxSendLength())
            throw error::Failure(make_error_code(TransportErrc::badTxLength));

        auto key = msg.requestKey();
        auto found = requests.find(key);
        if (found != requests.end())
        {
            post(std::move(found->second),
                 make_error_code(SessionErrc::cancelled), Message());
            requests.erase(found);
        }

        requests.emplace(msg.requestKey(), std::move(handler));

        trace(msg, true);
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
        auto s = state();
        if (s == State::establishing || s == State::authenticating ||
            s == State::established  || s == State::shuttingDown)
        {
            Variant v;
            if (checkError(decode(buffer, v)) &&
                check(v.is<Array>(), ProtocolErrc::badSchema))
            {
                std::error_code ec;
                auto msg = Message::parse(std::move(v.as<Array>()), ec);
                if (checkError(ec))
                {
                    trace(msg, false);
                    if (checkValidMsg(msg.type()))
                        processMessage(std::move(msg));
                }
            }
        }
    }

    std::error_code decode(const MessageBuffer& buffer, Variant& variant)
    {
        return decoder_.decode(buffer, variant);
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
                if (state() != State::shuttingDown)
                {
                    auto self = this->shared_from_this();
                    post(&Peer::onInbound, self, std::move(msg));
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
            post(std::move(handler), make_error_code(ProtocolErrc::success),
                 std::move(msg));
        }
        else
        {
            auto kv = multiShotRequestMap_.find(key);
            if (kv != multiShotRequestMap_.end())
            {
                if (msg.isProgressiveResponse())
                {
                    post(kv->second,
                         make_error_code(ProtocolErrc::success), std::move(msg));
                }
                else
                {
                    auto handler = std::move(kv->second);
                    multiShotRequestMap_.erase(kv);
                    post(std::move(handler),
                         make_error_code(ProtocolErrc::success), std::move(msg));
                }
            }
        }
    }

    void processHello(Message&& msg)
    {
        assert(state() == State::establishing);
        setState(State::established);
        auto self = this->shared_from_this();
        post(&Peer::onInbound, self, std::move(msg));
    }

    void processChallenge(Message&& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        auto self = this->shared_from_this();
        post(&Peer::onInbound, self, std::move(msg));
    }

    void processWelcome(Message&& msg)
    {
        assert((state() == State::establishing) ||
               (state() == State::authenticating));
        setState(State::established);
        processWampReply(RequestKey(WampMsgType::hello, 0), std::move(msg));
    }

    void processAbort(Message&& msg)
    {
        assert((state() == State::establishing) ||
               (state() == State::authenticating));
        setState(State::closed);
        processWampReply(RequestKey(WampMsgType::hello, 0), std::move(msg));
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
            const auto& reason = message_cast<GoodbyeMessage>(msg).reasonUri();
            SessionErrc errc;
            lookupWampErrorUri(reason, SessionErrc::closeRealm, errc);
            abortPending(make_error_code(errc));
            GoodbyeMessage msg("wamp.error.goodbye_and_out");
            send(msg);
            setState(State::closed);
        }
    }

    bool checkError(std::error_code ec)
    {
        bool success = !ec;
        if (!success)
            fail(ec);
        return success;
    }

    bool check(bool condition, ProtocolErrc errc)
    {
        if (!condition)
            fail(errc);
        return condition;
    }

    bool checkValidMsg(WampMsgType type)
    {
        auto traits = MessageTraits::lookup(type);
        bool valid = isMsgSupported(traits);

        if (!valid)
            fail(ProtocolErrc::unsupportedMsg);
        else
        {
            switch (state())
            {
            case State::establishing:
                valid = traits.forEstablishing;
                break;

            case State::authenticating:
                valid = traits.forChallenging;
                break;

            case State::established:
            case State::shuttingDown:
                valid = traits.forEstablished;
                break;

            default:
                valid = false;
                break;
            }

            if (!valid)
                fail(ProtocolErrc::unexpectedMsg);
        }

        return valid;
    }

    void fail(std::error_code ec)
    {
        setState(State::failed);
        abortPending(ec);
        transport_->close();
    }

    void abortPending(std::error_code ec, bool terminating = false)
    {
        if (!terminating)
        {
            for (auto& kv: oneShotRequestMap_)
                post(std::move(kv.second), ec, Message());
            for (auto& kv: multiShotRequestMap_)
                post(std::move(kv.second), ec, Message());
        }
        oneShotRequestMap_.clear();
        multiShotRequestMap_.clear();
    }

    void trace(const Message& msg, bool isTx)
    {
        if (traceHandler_)
        {
            std::ostringstream oss;
            oss << (isTx ? "Tx" : "Rx") << " message: [";
            if (!msg.fields().empty())
            {
                // Print message type field as {"NAME":<Field>} pair
                oss << "{\"" << msg.nameOr("INVALID") << "\":"
                    << msg.fields().at(0) << "}";

                for (Array::size_type i=1; i<msg.fields().size(); ++i)
                {
                    oss << "," << msg.fields().at(i);
                }
            }
            oss << ']';
            dispatchVia(userExecutor_, traceHandler_, oss.str());
        }
    }

    IoStrand strand_;
    AnyIoExecutor userExecutor_;
    TransportPtr transport_;
    EncoderType encoder_;
    DecoderType decoder_;
    LogHandler traceHandler_;
    StateChangeHandler stateChangeHandler_;
    std::atomic<State> state_;
    OneShotRequestMap oneShotRequestMap_;
    MultiShotRequestMap multiShotRequestMap_;
    RequestId nextRequestId_ = nullRequestId();

    static constexpr RequestId maxRequestId_ = 9007199254740992ull;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIALOGUE_HPP
