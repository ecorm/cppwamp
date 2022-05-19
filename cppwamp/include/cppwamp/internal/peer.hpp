/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DIALOGUE_HPP
#define CPPWAMP_INTERNAL_DIALOGUE_HPP

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include "../codec.hpp"
#include "../peerdata.hpp"
#include "../error.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "asynctask.hpp"
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

    State state() const {return state_;}

protected:
    using Message      = WampMessage;
    using Handler      = std::function<void (std::error_code, Message)>;
    using RxHandler    = std::function<void (Message)>;
    using LogHandler   = std::function<void (std::string)>;

    struct RequestOptions
    {
        RequestOptions()
            : progressiveResponse(false)
        {}

        RequestOptions& withProgressiveResponse(bool enabled)
        {
            progressiveResponse = enabled;
            return *this;
        }

        bool progressiveResponse;
    };

    Peer(TransportPtr&& transport)
        : transport_(std::move(transport))
    {
        initMessages();
    }

    virtual bool isMsgSupported(const MessageTraits& traits) = 0;

    virtual void onInbound(Message msg) = 0;

    void start()
    {
        assert(state_ == State::closed || state_ == State::disconnected);
        state_ = State::establishing;

        if (!transport_->isStarted())
        {
            std::weak_ptr<Peer> self(this->shared_from_this());

            transport_->start(
                [self](Buffer buf)
                {
                    auto me = self.lock();
                    if (me)
                        me->onTransportRx(buf);
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

    void adjourn(Reason reason, Handler handler)
    {
        assert(state_ == State::established);

        state_ = State::shuttingDown;
        using std::move;
        Message msg = { WampMsgType::goodbye,
                        {0u, move(reason.options({})), move(reason.uri({}))} };
        auto self = this->shared_from_this();
        request(msg,
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (!ec)
                {
                    state_ = State::closed;
                    abortPending(make_error_code(SessionErrc::sessionEnded));
                    post(handler, make_error_code(ProtocolErrc::success),
                         move(reply));
                }
                else
                    post(handler, ec, Message());
            });
    }

    void close(bool terminating)
    {
        state_ = State::disconnected;
        abortPending(make_error_code(SessionErrc::sessionEnded), terminating);
        transport_->close();
    }

    void send(Message& msg)
    {
        sendMessage(msg);
    }

    void sendError(WampMsgType reqType, RequestId reqId, Error failure)
    {
        using std::move;

        if (!failure.kwargs().empty())
        {
            auto& f = errorWithKwargsMsg_.fields;
            f.at(1) = static_cast<Int>(reqType);
            f.at(2) = reqId;
            f.at(3) = move(failure.options({}));
            f.at(4) = move(failure.reason({}));
            f.at(5) = move(failure.args({}));
            f.at(6) = move(failure.kwargs({}));
            send(errorWithKwargsMsg_);
        }
        else if (!failure.args().empty())
        {
            auto& f = errorWithArgsMsg_.fields;
            f.at(1) = static_cast<Int>(reqType);
            f.at(2) = reqId;
            f.at(3) = move(failure.options({}));
            f.at(4) = move(failure.reason({}));
            f.at(5) = move(failure.args({}));
            send(errorWithArgsMsg_);
        }
        else
        {
            auto& f = errorMsg_.fields;
            f.at(1) = static_cast<Int>(reqType);
            f.at(2) = reqId;
            f.at(3) = move(failure.options({}));
            f.at(4) = move(failure.reason({}));
            send(errorMsg_);
        }
    }

    RequestId request(Message& msg, Handler&& handler)
    {
        return sendMessage(msg, std::move(handler));
    }

    RequestId request(Message& msg, RequestOptions opts, Handler&& handler)
    {
        return sendMessage(msg, std::move(handler), opts);
    }

    template <typename TErrorValue>
    void fail(TErrorValue errc)
    {
        fail(make_error_code(errc));
    }

    void setTraceHandler(AsyncTask<std::string> handler)
        {traceHandler_ = std::move(handler);}

    template <typename TFunctor>
    void post(TFunctor&& fn) {transport_->post(std::forward<TFunctor>(fn));}

    template <typename TFunctor, typename... TArgs>
    void post(TFunctor&& fn, TArgs&&... args)
    {
        transport_->post(std::bind(std::forward<TFunctor>(fn),
                         std::forward<TArgs>(args)...));
    }

private:
    static constexpr unsigned progressiveResponseFlag_ = 0x01;

    struct RequestRecord
    {
        RequestRecord() {}

        RequestRecord(Handler&& h)
            : handler(std::move(h))
        {}

        RequestRecord(Handler&& h, RequestOptions opts)
            : handler(std::move(h)),
              options(opts)
        {}

        Handler handler;
        RequestOptions options;
    };

    using Buffer     = typename Transport::Buffer;
    using RequestKey = typename Message::RequestKey;
    using RequestMap = std::map<RequestKey, RequestRecord>;

    RequestId sendMessage(Message& msg, Handler&& handler = nullptr,
                          RequestOptions opts = {})
    {
        assert(msg.type != WampMsgType::none);

        msg.fields.at(0) = static_cast<Int>(msg.type);

        auto requestId = setMessageRequestId(msg);

        auto buf = newBuffer(msg);
        if (buf->length() > transport_->maxSendLength())
            throw error::Failure(make_error_code(TransportErrc::badTxLength));

        if (handler)
        {
            requestMap_[msg.requestKey()] =
                RequestRecord(std::move(handler), opts);
        }

        trace(msg, true);
        transport_->send(std::move(buf));

        return requestId;
    }

    RequestId setMessageRequestId(Message& msg)
    {
        RequestId requestId = 0;

        auto idPos = msg.traits().idPosition;
        if (idPos > 0)
        {
            requestId = nextRequestId();
            msg.fields.at(idPos) = requestId;
        }

        return requestId;
    }

    RequestId nextRequestId()
    {
        if (nextRequestId_ >= maxRequestId_)
            nextRequestId_ = 0;
        return ++nextRequestId_;
    }

    Buffer newBuffer(const Message& msg)
    {
        Buffer buf = transport_->getBuffer();
        Codec::encodeBuffer(msg.fields, *buf);
        return buf;
    }

    void onTransportRx(Buffer buf)
    {
        if (state_ == State::establishing ||
            state_ == State::authenticating ||
            state_ == State::established ||
            state_ == State::shuttingDown)
        {
            Variant v;
            if (checkError(decode(buf, v)) &&
                check(v.is<Array>(), ProtocolErrc::badSchema))
            {
                std::error_code ec;
                auto msg = Message::parse(std::move(v.as<Array>()), ec);
                if ( checkError(ec) && checkValidMsg(msg.type) )
                {
                    trace(msg, false);
                    processMessage(std::move(msg));
                }
            }
        }
    }

    std::error_code decode(const Buffer& buf, Variant& v)
    {
        auto result = make_error_code(ProtocolErrc::success);
        try
        {
            Codec::decodeBuffer(*buf, v);
        }
        catch(const error::Decode& e)
        {
            result = make_error_code(ProtocolErrc::badDecode);
        }
        return result;
    }

    void processMessage(Message&& msg)
    {
        if (msg.repliesTo() != WampMsgType::none)
        {
            processWampReply( RequestKey(msg.repliesTo(), msg.requestId()),
                              std::move(msg) );
        }
        else switch(msg.type)
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
                if (state_ != State::shuttingDown)
                {
                    auto self = this->shared_from_this();
                    post(std::bind(&Peer::onInbound, self, std::move(msg)));
                }
                break;
        }
    }

    void processWampReply(const RequestKey& key, Message&& msg)
    {
        auto kv = requestMap_.find(key);
        if (kv != requestMap_.end())
        {
            if (msg.isProgressiveResponse())
            {
                const auto& handler = std::move(kv->second.handler);
                post(handler, make_error_code(ProtocolErrc::success),
                     std::move(msg));
            }
            else
            {
                auto handler = std::move(kv->second.handler);
                requestMap_.erase(kv);
                post(std::move(handler), make_error_code(ProtocolErrc::success),
                     std::move(msg));
            }
        }
    }

    void processHello(Message&& msg)
    {
        assert(state_ == State::establishing);
        state_ = State::established;
        auto self = this->shared_from_this();
        post(std::bind(&Peer::onInbound, self, std::move(msg)));
    }

    void processChallenge(Message&& msg)
    {
        assert(state_ == State::establishing);
        state_ = State::authenticating;
        auto self = this->shared_from_this();
        post(std::bind(&Peer::onInbound, self, std::move(msg)));
    }

    void processWelcome(Message&& msg)
    {
        assert((state_ == State::establishing) ||
               (state_ == State::authenticating));
        state_ = State::established;
        processWampReply( RequestKey(WampMsgType::hello, msg.requestId()),
                          std::move(msg) );
    }

    void processAbort(Message&& msg)
    {
        assert((state_ == State::establishing) ||
               (state_ == State::authenticating));
        state_ = State::closed;
        processWampReply( RequestKey(WampMsgType::hello, msg.requestId()),
                          std::move(msg) );
    }

    void processGoodbye(Message&& msg)
    {
        if (state_ == State::shuttingDown)
        {
            state_ = State::closed;
            processWampReply(msg.requestKey(), std::move(msg));
        }
        else
        {
            auto reason = msg.fields.at(2).as<String>();
            auto errc = lookupWampErrorUri(reason, SessionErrc::closeRealm);
            abortPending(make_error_code(errc));
            Message reply{ WampMsgType::goodbye,
                           {0u, Object{}, "wamp.error.goodbye_and_out"} };
            send(reply);
            state_ = State::closed;
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
            switch (state_)
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
        state_ = State::failed;
        abortPending(ec);
        transport_->close();
    }

    void abortPending(std::error_code ec, bool terminating = false)
    {
        if (!terminating)
            for (const auto& kv: requestMap_)
                post(std::bind(kv.second.handler, ec, Message()));
        requestMap_.clear();
    }

    void trace(const Message& msg, bool isTx)
    {
        if (traceHandler_)
        {
            std::ostringstream oss;
            oss << (isTx ? "Tx" : "Rx") << " message: " << msg.fields;
            traceHandler_(oss.str());
        }
    }

    void initMessages()
    {
        using M = Message;
        using T = WampMsgType;

        Int n = 0;
        String s;
        Array a;
        Object o;

        errorMsg_           = M{ T::error, {n, n, n, o, s} };
        errorWithArgsMsg_   = M{ T::error, {n, n, n, o, s, a} };
        errorWithKwargsMsg_ = M{ T::error, {n, n, n, o, s, a, o} };
    }

    TransportPtr transport_;
    AsyncTask<std::string> traceHandler_;
    State state_ = State::closed;
    RequestMap requestMap_;
    RequestId nextRequestId_ = 0;
    Buffer rxBuffer_;

    Message errorMsg_;
    Message errorWithArgsMsg_;
    Message errorWithKwargsMsg_;

    static constexpr RequestId maxRequestId_ = 9007199254740992ull;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIALOGUE_HPP
