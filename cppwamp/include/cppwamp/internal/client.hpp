/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENT_HPP
#define CPPWAMP_INTERNAL_CLIENT_HPP

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include "../codec.hpp"
#include "../json.hpp"
#include "../msgpack.hpp"
#include "../version.hpp"
#include "clientinterface.hpp"
#include "dialogue.hpp"
#include "registrationimpl.hpp"
#include "subscriptionimpl.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Provides the implementation of the wamp::Session class.
//------------------------------------------------------------------------------
template <typename TCodec, typename TTransport>
class Client : public ClientInterface, public Dialogue<TCodec, TTransport>
{
public:
    using Ptr          = std::shared_ptr<Client>;
    using WeakPtr      = std::weak_ptr<Client>;
    using Codec        = TCodec;
    using Transport    = TTransport;
    using TransportPtr = std::shared_ptr<Transport>;
    using State        = SessionState;

    static Ptr create(TransportPtr&& transport)
        {return Ptr(new Client(std::move(transport)));}

    virtual ~Client() override {terminate();}

    virtual State state() const override {return Base::state();}

    virtual void join(Realm&& realm, AsyncHandler<SessionInfo> handler) override
    {
        using std::move;

        realm.withOption("agent", Version::agentString())
             .withOption("roles", roles());

        String realmUri = realm.uri();

        Message msg = { WampMsgType::hello,
                        {0u, move(realm.uri({})), move(realm.options({}))} };
        this->start();
        auto self = this->shared_from_this();
        this->request(msg,
            [this, self, handler, realmUri](std::error_code ec, Message reply)
            {
                if (checkError(ec, handler))
                {
                    if (reply.type == WampMsgType::welcome)
                    {
                        this->post(handler,
                                   SessionInfo({},
                                               move(realmUri),
                                               move(reply.as<Int>(1)),
                                               move(reply.as<Object>(2))));
                    }
                    else
                    {
                        assert(reply.type == WampMsgType::abort);
                        const auto& uri = reply.as<String>(2);
                        auto errc = lookupWampErrorUri(uri,
                                                       SessionErrc::joinError);

                        std::ostringstream oss;
                        oss << "with URI=" << uri;
                        if (!reply.as<Object>(1).empty())
                            oss << ", Details=" << reply.at(1);

                        AsyncResult<SessionInfo> result(make_error_code(errc),
                                                        oss.str());
                        this->post(handler, move(result));
                    }
                }
            });
    }

    virtual void leave(Reason&& reason,
                       AsyncHandler<Reason>&& handler) override
    {
        using std::move;
        if (reason.uri().empty())
            reason.uri({}) = "wamp.error.close_realm";
        auto self = this->shared_from_this();
        Base::adjourn(move(reason),
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkError(ec, handler))
                {
                    auto reason = Reason(move(reply.as<String>(2)))
                                    .withOptions(move(reply.as<Object>(1)));
                    this->post(handler, move(reason));
                }
                readership_.clear();
                registry_.clear();
            });
    }

    virtual void disconnect() override
    {
        this->close(false);
    }

    virtual void terminate() override
    {
        setLogHandlers(nullptr, nullptr);
        this->close(true);
    }

    virtual void subscribe(Subscription::Ptr sub,
                           AsyncHandler<Subscription::Ptr>&& handler) override
    {
        auto kv = topics_.find(sub->topic().uri());
        if (kv == topics_.end())
        {
            subscribeMsg_.at(2) = sub->topic().options();
            subscribeMsg_.at(3) = sub->topic().uri();
            auto self = this->shared_from_this();
            this->request(subscribeMsg_,
                [this, self, sub, handler](std::error_code ec, Message reply)
                {
                    if (checkReply(WampMsgType::subscribed, ec, reply,
                                   SessionErrc::subscribeError, handler))
                    {
                        auto subId = reply.to<SubscriptionId>(2);
                        sub->setId(subId, {});
                        topics_.emplace(sub->topic().uri(), subId);
                        readership_[subId].emplace(sub.get(), sub);
                        this->post(handler, sub);
                    }
                });
        }
        else
        {
            auto subId = kv->second;
            sub->setId(subId, {});
            readership_[subId].emplace(sub.get(), sub);
            this->post(handler, sub);
        }
    }

    virtual void unsubscribe(Subscription* sub) override
    {
        assert(sub);
        auto kv = readership_.find(sub->id());
        if (kv != readership_.end())
        {
            auto& subs = kv->second;
            if (!subs.empty())
            {
                subs.erase(sub);
                if (subs.empty())
                {
                    topics_.erase(sub->topic().uri());
                        sendUnsubscribe(sub->id());
                }
            }
        }
    }

    virtual void unsubscribe(Subscription* sub,
                             AsyncHandler<bool> handler) override
    {
        assert(sub);
        bool unsubscribed = false;
        auto kv = readership_.find(sub->id());
        if (kv != readership_.end())
        {
            auto& subs = kv->second;
            if (!subs.empty())
            {
                subs.erase(sub);
                unsubscribed = true;
                if (subs.empty())
                {
                    topics_.erase(sub->topic().uri());
                    sendUnsubscribe(sub->id(), std::move(handler));
                    handler = nullptr;
                }
            }
        }

        if (handler)
            this->post(handler, unsubscribed);
    }

    virtual void publish(Pub&& pub) override
    {
        this->send(marshallPublish(std::move(pub)));
    }

    virtual void publish(Pub&& pub,
                         AsyncHandler<PublicationId>&& handler) override
    {
        pub.options({}).emplace("acknowledge", true);
        auto self = this->shared_from_this();
        this->request(marshallPublish(std::move(pub)),
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkReply<PublicationId>(WampMsgType::published, ec, reply,
                        SessionErrc::publishError, handler))
                {
                    this->post(handler, reply.to<PublicationId>(2));
                }
            });
    }

    virtual void enroll(Registration::Ptr reg,
                        AsyncHandler<Registration::Ptr>&& handler) override
    {
        enrollMsg_.at(2) = reg->procedure().options();
        enrollMsg_.at(3) = reg->procedure().uri();
        auto self = this->shared_from_this();
        this->request(enrollMsg_,
            [this, self, reg, handler](std::error_code ec, Message reply)
            {
                if (checkReply<Registration::Ptr>(WampMsgType::registered, ec,
                        reply, SessionErrc::registerError, handler))
                {
                    auto regId = reply.to<RegistrationId>(2);
                    reg->setId(regId, {});
                    registry_[regId] = reg;
                    this->post(handler, reg);
                }
            });
    }

    virtual void unregister(RegistrationId regId) override
    {
        auto kv = registry_.find(regId);
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            if (state() == State::established)
            {
                unregisterMsg_.at(2) = regId;
                auto self = this->shared_from_this();
                this->request( unregisterMsg_,
                    [this, self](std::error_code ec, Message reply)
                    {
                        // Don't propagate WAMP errors, as we prefer this
                        // to be a no-fail cleanup operation.
                        warnReply(WampMsgType::unregistered, ec, reply,
                                  SessionErrc::unregisterError);
                    });
            }
        }
    }

    virtual void unregister(RegistrationId regId,
                            AsyncHandler<bool> handler) override
    {
        CPPWAMP_LOGIC_CHECK(state() == State::established,
                            "Session is not established");
        auto kv = registry_.find(regId);
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            unregisterMsg_.at(2) = regId;
            auto self = this->shared_from_this();
            this->request( unregisterMsg_,
                [this, self, handler](std::error_code ec, Message reply)
                {
                    if (checkReply(WampMsgType::unregistered, ec, reply,
                                   SessionErrc::unregisterError, handler))
                        this->post(handler, true);
                });
        }
        else
            this->post(handler, false);
    }

    virtual void call(Rpc&& rpc, AsyncHandler<Result>&& handler) override
    {
        using std::move;
        if (!rpc.kwargs().empty())
        {
            callWithKwargsMsg_.at(2) = move(rpc.options({}));
            callWithKwargsMsg_.at(3) = move(rpc.procedure({}));
            callWithKwargsMsg_.at(4) = move(rpc.args({}));
            callWithKwargsMsg_.at(5) = move(rpc.kwargs({}));
            callProcedure(callWithKwargsMsg_, move(handler));
        }
        else if (!rpc.args().empty())
        {
            callWithArgsMsg_.at(2) = move(rpc.options({}));
            callWithArgsMsg_.at(3) = move(rpc.procedure({}));
            callWithArgsMsg_.at(4) = move(rpc.args({}));
            callProcedure(callWithArgsMsg_, move(handler));
        }
        else
        {
            callMsg_.at(2) = move(rpc.options({}));
            callMsg_.at(3) = move(rpc.procedure({}));
            callProcedure(callMsg_, move(handler));
        }
    }

    virtual void yield(RequestId reqId, Result&& result) override
    {
        using std::move;
        if (!result.kwargs().empty())
        {
            yieldWithKwargsMsg_.at(1) = reqId;
            yieldWithKwargsMsg_.at(2) = move(result.options({}));
            yieldWithKwargsMsg_.at(3) = move(result.args({}));
            yieldWithKwargsMsg_.at(4) = move(result.kwargs({}));
            this->send(yieldWithKwargsMsg_);
        }
        else if (!result.args().empty())
        {
            yieldWithArgsMsg_.at(1) = reqId;
            yieldWithArgsMsg_.at(2) = move(result.options({}));
            yieldWithArgsMsg_.at(3) = move(result.args({}));
            this->send(yieldWithArgsMsg_);
        }
        else
        {
            yieldMsg_.at(1) = reqId;
            yieldMsg_.at(2) = move(result.options({}));
            this->send(yieldMsg_);
        }
    }

    virtual void yield(RequestId reqId, Error&& failure) override
    {
        using std::move;
        this->sendError(WampMsgType::invocation, reqId, move(failure));
    }

    virtual void setLogHandlers(LogHandler warningHandler,
                                LogHandler traceHandler) override
    {
        warningHandler_ = std::move(warningHandler);
        this->setTraceHandler(std::move(traceHandler));
    }

    virtual void postpone(std::function<void ()> functor) override
    {
        this->post(functor);
    }

private:
    using Base        = Dialogue<Codec, Transport>;
    using WampMsgType = internal::WampMsgType;
    using Message     = internal::WampMessage;
    using Subscribers = std::map<Subscription*, Subscription::WeakPtr>;
    using Readership  = std::map<SubscriptionId, Subscribers>;
    using TopicMap    = std::map<std::string, SubscriptionId>;
    using Registry    = std::map<RegistrationId, Registration::WeakPtr>;

    Client(TransportPtr transport)
        : Base(std::move(transport))
    {
        warningHandler_ = [](const std::string& log)
        {
            std::cerr << "[CppWAMP] Warning: " << log << "\n";
        };

        initMessages();
    }

    Ptr shared_from_this()
    {
        return std::static_pointer_cast<Client>( Base::shared_from_this() );
    }

    void sendUnsubscribe(SubscriptionId subId)
    {
        if (state() == State::established)
        {
            unsubscribeMsg_.at(2) = subId;
            auto self = this->shared_from_this();
            this->request( unsubscribeMsg_,
                [this, self](std::error_code ec, Message reply)
                {
                    // Don't propagate WAMP errors, as we prefer
                    // this to be a no-fail cleanup operation.
                    warnReply(WampMsgType::unsubscribed, ec, reply,
                              SessionErrc::unsubscribeError);
                });
        }
    }

    void sendUnsubscribe(SubscriptionId subId, UnsubscribeHandler&& handler)
    {
        CPPWAMP_LOGIC_CHECK((this->state() == State::established),
                            "Session is not established");
        unsubscribeMsg_.at(2) = subId;
        auto self = this->shared_from_this();
        this->request( unsubscribeMsg_,
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkReply(WampMsgType::unsubscribed, ec, reply,
                               SessionErrc::unsubscribeError, handler))
                    this->post(handler, true);
            });
    }

    Message& marshallPublish(Pub&& pub)
    {
        using std::move;
        if (!pub.kwargs().empty())
        {
            publishKwargsMsg_.at(2) = move(pub.options({}));
            publishKwargsMsg_.at(3) = move(pub.topic({}));
            publishKwargsMsg_.at(4) = move(pub.args({}));
            publishKwargsMsg_.at(5) = move(pub.kwargs({}));
            return publishKwargsMsg_;
        }
        else if (!pub.args().empty())
        {
            publishArgsMsg_.at(2) = move(pub.options({}));
            publishArgsMsg_.at(3) = move(pub.topic({}));
            publishArgsMsg_.at(4) = move(pub.args({}));
            return publishArgsMsg_;
        }
        else
        {
            publishMsg_.at(2) = move(pub.options({}));
            publishMsg_.at(3) = move(pub.topic({}));
            return publishMsg_;
        }
    }

    void callProcedure(Message& msg, AsyncHandler<Result>&& handler)
    {
        auto self = this->shared_from_this();
        this->request(msg,
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkReply<Result>(WampMsgType::result, ec, reply,
                                       SessionErrc::callError, handler))
                {
                    using std::move;
                    Result result({}, reply.to<RequestId>(1),
                                  move(reply.as<Object>(2)));

                    if (reply.size() >= 4)
                        result.withArgs(move(reply.as<Array>(3)));
                    if (reply.size() >= 5)
                        result.withKwargs(move(reply.as<Object>(4)));
                    this->post(handler, std::move(result));
                }
            });
    }

    virtual bool isMsgSupported(const MessageTraits& traits) override
    {
        return traits.isClientRx;
    }

    virtual void onInbound(Message msg) override
    {
        switch (msg.type)
        {
        case WampMsgType::event:
            onEvent(std::move(msg));
            break;

        case WampMsgType::invocation:
            onInvocation(std::move(msg));
            break;

        default:
            assert(false);
        }
    }

    void onEvent(Message&& msg)
    {
        using std::move;

        auto subId = msg.to<SubscriptionId>(1);
        auto kv = readership_.find(subId);
        if (kv != readership_.end())
        {
            Event event({}, subId, msg.to<PublicationId>(2),
                        move(move(msg.as<Object>(3))));
            if (msg.fields.size() >= 5)
                event.args({}) = move(msg.as<Array>(4));
            if (msg.fields.size() >= 6)
                event.kwargs({}) = move(msg.as<Object>(5));

            auto self = this->shared_from_this();
            const auto& subscribers = kv->second;
            if (subscribers.size() == 1)
            {
                this->post(&Client::dispatchEvent, self,
                           subscribers.begin()->second, move(event));
            }
            else
            {
                for (auto& subKv: subscribers)
                    this->post(&Client::dispatchEvent, self,
                               subKv.second, event);
            }
        }
    }

    void dispatchEvent(Subscription::WeakPtr subscription, Event event)
    {
        // Copy the publication ID before the Event object gets moved away.
        auto pubId = event.pubId();

        auto sub = subscription.lock();
        if (sub)
        {
            const char* warningMsg = nullptr;
            try
            {
                sub->invoke(std::move(event), {});
            }
            catch (const std::out_of_range&)
            {
                warningMsg = "Received an EVENT with insufficient positional "
                             "arguments";
            }
            catch (const error::Conversion&)
            {
                warningMsg = "Received an EVENT with invalid positional "
                             "arguments types";
            }
            if (warningMsg && warningHandler_)
            {
                std::ostringstream oss;
                oss << warningMsg << " (with topic=\"" << sub->topic().uri()
                    << "\", subId=" << sub->id() << " pubId=" << pubId << ")";
                warn(oss.str());
            }
        }
        else if (warningHandler_)
        {
            std::ostringstream oss;
            oss << "Received an EVENT that is no longer subscribed for "
                   "(with subId=" << event.subId() << " pubId=" << pubId << ")";
            warn(oss.str());
        }

    }

    void onInvocation(Message&& msg)
    {
        using std::move;

        auto requestId = msg.to<RequestId>(1);
        auto regId = msg.to<RegistrationId>(2);
        auto kv = registry_.find(regId);
        if (kv != registry_.end())
        {
            auto self = this->shared_from_this();
            Invocation inv({}, self, requestId, move(msg.as<Object>(3)));
            if (msg.fields.size() >= 5)
                inv.args({}) = move(msg.as<Array>(4));
            if (msg.fields.size() >= 6)
                inv.kwargs({}) = move(msg.as<Object>(5));

            this->post(&Client::dispatchInvocation, self,
                       kv->second, move(inv));
        }
        else
        {
            this->sendError(WampMsgType::invocation, requestId,
                    Error("wamp.error.no_such_procedure")
                        .withArgs({"The called procedure does not exist"}));
        }
    }

    void dispatchInvocation(Registration::WeakPtr registration,
                            Invocation invocation)
    {
        // Copy the request ID before the Invocation object gets moved away.
        auto reqId = invocation.requestId();

        auto reg = registration.lock();
        if (reg)
        {
            try
            {
                reg->invoke(std::move(invocation), {});
            }
            catch (const std::out_of_range&)
            {
                this->sendError(WampMsgType::invocation, reqId,
                        Error("wamp.error.invalid_argument")
                            .withArgs({"Insufficient arguments"}));
            }
            catch (const error::Conversion& e)
            {
                this->sendError(WampMsgType::invocation, reqId,
                        Error("wamp.error.invalid_argument")
                            .withArgs({"Argument type mismatch", e.what()}));
            }
        }
        else
        {
            this->sendError(WampMsgType::invocation, reqId,
                    Error("wamp.error.no_such_procedure")
                        .withArgs({"The called procedure no longer exists"}));
        }
    }

    template <typename THandler>
    bool checkError(std::error_code ec, THandler& handler)
    {
        if (ec)
            this->post(handler, ec);
        return !ec;
    }

    template <typename TResult>
    bool checkReply(WampMsgType type, std::error_code ec, const Message& reply,
                    SessionErrc defaultErrc, const AsyncHandler<TResult>& handler)
    {
        bool success = checkError(ec, handler);
        if (success)
        {
            if (reply.type == WampMsgType::error)
            {
                success = false;
                const auto& uri = reply.as<String>(4);
                auto errc = lookupWampErrorUri(uri, defaultErrc);

                std::ostringstream oss;
                oss << "with URI=" << uri;
                if (reply.size() >= 6 && !reply.as<Array>(5).empty())
                    oss << ", Args=" << reply.at(5);
                if (reply.size() >= 7 && !reply.as<Object>(6).empty())
                    oss << ", ArgsKv=" << reply.at(6);

                AsyncResult<TResult> result(make_error_code(errc), oss.str());
                this->post(handler, result);
            }
            else
                assert((reply.type == type) && "Unexpected WAMP message type");
        }
        return success;
    }

    void warnReply(WampMsgType type, std::error_code ec, const Message& reply,
                   SessionErrc defaultErrc)
    {
        auto self = this->shared_from_this();
        checkReply<bool>(type, ec, reply, defaultErrc,
            [this, self](AsyncResult<bool> result)
            {
                if (!result)
                    warn(error::Failure::makeMessage(result.errorCode(),
                                                     result.errorInfo()));
            });
    }

    void warn(const std::string& log)
    {
        if (warningHandler_)
            warningHandler_(log);
    }

    void initMessages()
    {
        using M = Message;
        using T = WampMsgType;

        Int n = 0;
        String s;
        Array a;
        Object o;

        publishMsg_            = M{ T::publish,     {n, n, o, s} };
        publishArgsMsg_        = M{ T::publish,     {n, n, o, s, a} };
        publishKwargsMsg_      = M{ T::publish,     {n, n, o, s, a, o} };
        subscribeMsg_          = M{ T::subscribe,   {n, n, o, s} };
        unsubscribeMsg_        = M{ T::unsubscribe, {n, n, n} };
        enrollMsg_             = M{ T::enroll,      {n, n, o, s} };
        unregisterMsg_         = M{ T::unregister,  {n, n, n} };
        callMsg_               = M{ T::call,        {n, n, o, s} };
        callWithArgsMsg_       = M{ T::call,        {n, n, o, s, a} };
        callWithKwargsMsg_     = M{ T::call,        {n, n, o, s, a, o} };
        yieldMsg_              = M{ T::yield,       {n, n, o} };
        yieldWithArgsMsg_      = M{ T::yield,       {n, n, o, a} };
        yieldWithKwargsMsg_    = M{ T::yield,       {n, n, o, a, o} };
    }

    TopicMap topics_;
    Readership readership_;
    Registry registry_;
    LogHandler warningHandler_;

    Message publishMsg_;
    Message publishArgsMsg_;
    Message publishKwargsMsg_;
    Message subscribeMsg_;
    Message unsubscribeMsg_;
    Message enrollMsg_;
    Message unregisterMsg_;
    Message callMsg_;
    Message callWithArgsMsg_;
    Message callWithKwargsMsg_;
    Message yieldMsg_;
    Message yieldWithArgsMsg_;
    Message yieldWithKwargsMsg_;
};


//------------------------------------------------------------------------------
template <typename TTransportPtr>
ClientInterface::Ptr createClient(CodecId codecId, TTransportPtr&& trn)
{
    using Transport = typename TTransportPtr::element_type;
    switch (codecId)
    {
    case CodecId::json:
        return Client<Json, Transport>::create(std::move(trn));

    case CodecId::msgpack:
        return Client<Msgpack, Transport>::create(std::move(trn));

    default:
        assert(false && "Unexpected CodecId");
    }
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
