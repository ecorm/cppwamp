/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENTIMPL_HPP
#define CPPWAMP_INTERNAL_CLIENTIMPL_HPP

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
#include "../args.hpp"
#include "../asyncresult.hpp"
#include "../codec.hpp"
#include "../error.hpp"
#include "../json.hpp"
#include "../msgpack.hpp"
#include "../invocation.hpp"
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "clientimplbase.hpp"
#include "registrationimpl.hpp"
#include "session.hpp"
#include "subscriptionimpl.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TCodec, typename TTransport>
class ClientImpl : public ClientImplBase, public Session<TCodec, TTransport>
{
public:
    using Ptr          = std::shared_ptr<ClientImpl>;
    using WeakPtr      = std::weak_ptr<ClientImpl>;
    using Codec        = TCodec;
    using Transport    = TTransport;
    using TransportPtr = std::shared_ptr<Transport>;
    using State        = SessionState;

    static Ptr create(TransportPtr&& transport)
        {return Ptr(new ClientImpl(std::move(transport)));}

    virtual ~ClientImpl() override {terminate();}

    virtual State state() const override {return Base::state();}

    virtual const std::string& realm() const override {return Base::realm();}

    virtual const Object& peerInfo() const override {return peerInfo_;}

    virtual void join(std::string realm,
                      AsyncHandler<SessionId> handler) override
    {
        using std::move;

        Object details{ { "roles", Object{
            {"caller", Object{}},
            {"callee", Object{}},
            {"publisher", Object{}},
            {"subscriber", Object{}},
        } } };

        Message msg = { WampMsgType::hello, {0u, realm, std::move(details)} };
        this->start(std::move(realm));
        auto self = this->shared_from_this();
        this->request(msg,
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkError(ec, handler))
                {
                    if (reply.type == WampMsgType::welcome)
                    {
                        peerInfo_ = move(reply.as<Object>(2));
                        this->post(handler, reply.to<SessionId>(1));
                    }
                    else
                    {
                        assert(reply.type == WampMsgType::abort);
                        WampErrc errc = WampErrc::joinError;
                        const auto& uri = reply.as<String>(2);
                        lookupWampErrorUri(uri, errc);

                        std::ostringstream oss;
                        oss << "with URI=" << uri;
                        if (!reply.as<Object>(1).empty())
                            oss << ", Details=" << reply.at(1);

                        AsyncResult<SessionId> result(make_error_code(errc),
                                                      oss.str());
                        this->post(handler, std::move(result));
                    }
                }
            });
    }

    virtual void leave(AsyncHandler<std::string>&& handler) override
    {
        leave("wamp.error.close_realm", std::move(handler));
    }

    virtual void leave(std::string&& reason,
                       AsyncHandler<std::string>&& handler) override
    {
        using std::move;
        auto self = this->shared_from_this();
        Base::adjourn(move(reason), Object(),
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkError(ec, handler))
                    this->post(handler, move(reply.as<String>(2)));
                readership_.clear();
                registry_.clear();
                peerInfo_.clear();
            });
    }

    virtual void disconnect() override
    {
        peerInfo_.clear();
        this->close(false);
    }

    virtual void terminate() override
    {
        peerInfo_.clear();
        setLogHandlers(nullptr, nullptr);
        this->close(true);
    }

    virtual void subscribe(SubscriptionBase::Ptr sub,
                           AsyncHandler<Subscription>&& handler) override
    {
        auto kv = topics_.find(sub->topic());
        if (kv == topics_.end())
        {
            subscribeMsg_.at(3) = sub->topic();
            auto self = this->shared_from_this();
            this->request(subscribeMsg_,
                [this, self, sub, handler](std::error_code ec, Message reply)
                {
                    if (checkReply(WampMsgType::subscribed, ec, reply,
                                   WampErrc::subscribeError, handler))
                    {
                        auto subId = reply.to<SubscriptionId>(2);
                        sub->setId(subId);
                        topics_.emplace(sub->topic(), subId);
                        readership_[subId].emplace(sub.get(), sub);
                        this->post(handler, Subscription(sub));
                    }
                });
        }
        else
        {
            auto subId = kv->second;
            sub->setId(subId);
            readership_[subId].emplace(sub.get(), sub);
            this->post(handler, Subscription(sub));
        }
    }

    virtual void unsubscribe(SubscriptionBase* sub) override
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
                    topics_.erase(sub->topic());
                        sendUnsubscribe(sub->id());
                }
            }
        }
    }

    virtual void unsubscribe(SubscriptionBase* sub,
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
                    topics_.erase(sub->topic());
                    sendUnsubscribe(sub->id(), std::move(handler));
                    handler = nullptr;
                }
            }
        }

        if (handler)
            this->post(handler, unsubscribed);
    }

    virtual void publish(std::string&& topic) override
    {
        publishMsg_.at(3) = std::move(topic);
        this->send(publishMsg_);
    }

    virtual void publish(std::string&& topic, Args&& args) override
    {
        using std::move;
        if (!args.map.empty())
        {
            publishKvArgsMsg_.at(3) = move(topic);
            publishKvArgsMsg_.at(4) = move(args.list);
            publishKvArgsMsg_.at(5) = move(args.map);
            this->send(publishKvArgsMsg_);
        }
        else if (!args.list.empty())
        {
            publishArgsMsg_.at(3) = move(topic);
            publishArgsMsg_.at(4) = move(args.list);
            this->send(publishArgsMsg_);
        }
        else
            publish(move(topic));
    }

    virtual void publish(std::string&& topic,
                         AsyncHandler<PublicationId>&& handler) override
    {
        ackedPublishMsg_.at(3) = std::move(topic);
        ackedPublish(ackedPublishMsg_, move(handler));
    }

    virtual void publish(std::string&& topic, Args&& args,
                         AsyncHandler<PublicationId>&& handler) override
    {
        using std::move;
        if (!args.map.empty())
        {
            ackedPublishKvArgsMsg_.at(3) = move(topic);
            ackedPublishKvArgsMsg_.at(4) = move(args.list);
            ackedPublishKvArgsMsg_.at(5) = move(args.map);
            ackedPublish(ackedPublishKvArgsMsg_, move(handler));
        }
        else if (!args.list.empty())
        {
            ackedPublishArgsMsg_.at(3) = move(topic);
            ackedPublishArgsMsg_.at(4) = move(args.list);
            ackedPublish(ackedPublishArgsMsg_, move(handler));
        }
        else
            publish(move(topic), move(handler));
    }

    virtual void enroll(RegistrationBase::Ptr reg,
                        AsyncHandler<Registration>&& handler) override
    {
        enrollMsg_.at(3) = reg->procedure();
        auto self = this->shared_from_this();
        this->request(enrollMsg_,
            [this, self, reg, handler](std::error_code ec, Message reply)
            {
                if (checkReply<Registration>(WampMsgType::registered, ec, reply,
                                             WampErrc::registerError, handler))
                {
                    auto regId = reply.to<RegistrationId>(2);
                    reg->setId(regId);
                    registry_[regId] = reg;
                    this->post(handler, Registration(reg));
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
                                  WampErrc::unregisterError);
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
                                   WampErrc::unregisterError, handler))
                        this->post(handler, true);
                });
        }
        else
            this->post(handler, false);
    }

    virtual void call(std::string&& procedure,
                      AsyncHandler<Args>&& handler) override
    {
        callMsg_.at(3) = std::move(procedure);
        callProcedure(callMsg_, std::move(handler));
    }

    virtual void call(std::string&& procedure, Args&& args,
                      AsyncHandler<Args>&& handler) override
    {
        using std::move;
        if (!args.map.empty())
        {
            callWithKvArgsMsg_.at(3) = move(procedure);
            callWithKvArgsMsg_.at(4) = move(args.list);
            callWithKvArgsMsg_.at(5) = move(args.map);
            callProcedure(callWithKvArgsMsg_, move(handler));
        }
        else if (!args.list.empty())
        {
            callWithArgsMsg_.at(3) = move(procedure);
            callWithArgsMsg_.at(4) = move(args.list);
            callProcedure(callWithArgsMsg_, move(handler));
        }
        else
            call(move(procedure), move(handler));
    }

    virtual void yield(RequestId reqId) override
    {
        yieldMsg_.at(1) = reqId;
        this->send(yieldMsg_);
    }

    virtual void yield(RequestId reqId, Args&& args) override
    {
        using std::move;
        if (!args.map.empty())
        {
            yieldWithKvArgsMsg_.at(1) = reqId;
            yieldWithKvArgsMsg_.at(3) = move(args.list);
            yieldWithKvArgsMsg_.at(4) = move(args.map);
            this->send(yieldWithKvArgsMsg_);
        }
        else if (!args.list.empty())
        {
            yieldWithArgsMsg_.at(1) = reqId;
            yieldWithArgsMsg_.at(3) = move(args.list);
            this->send(yieldWithArgsMsg_);
        }
        else
            yield(reqId);
    }

    virtual void fail(RequestId reqId, std::string&& reason, Object&& details,
                      Args&& args) override
    {
        using std::move;
        this->sendError(WampMsgType::invocation, reqId, move(reason),
                        move(details), move(args));
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
    using Base        = Session<Codec, Transport>;
    using WampMsgType = internal::WampMsgType;
    using Message     = internal::WampMessage;
    using Subscribers = std::map<SubscriptionBase*, SubscriptionBase::WeakPtr>;
    using Readership  = std::map<SubscriptionId, Subscribers>;
    using TopicMap    = std::map<std::string, SubscriptionId>;
    using Registry    = std::map<RegistrationId, RegistrationBase::WeakPtr>;

    ClientImpl(TransportPtr transport)
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
        return std::static_pointer_cast<ClientImpl>( Base::shared_from_this() );
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
                              WampErrc::unsubscribeError);
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
                               WampErrc::unsubscribeError, handler))
                    this->post(handler, true);
            });
    }

    void ackedPublish(Message& msg, AsyncHandler<PublicationId>&& handler)
    {
        auto self = this->shared_from_this();
        this->request(msg,
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkReply<PublicationId>(WampMsgType::published, ec, reply,
                                              WampErrc::publishError, handler))
                {
                    this->post(handler, reply.to<PublicationId>(2));
                }
            });
    }

    void callProcedure(Message& msg, AsyncHandler<Args>&& handler)
    {
        auto self = this->shared_from_this();
        this->request(msg,
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkReply<Args>(WampMsgType::result, ec, reply,
                                     WampErrc::callError, handler))
                {
                    Args result;
                    if (reply.size() >= 4)
                        result.list = move(reply.as<Array>(3));
                    if (reply.size() >= 5)
                        result.map = move(reply.as<Object>(4));
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
            auto pubId = msg.to<PublicationId>(2);
            Args args;
            if (msg.fields.size() >= 5)
                args.list = move(msg.as<Array>(4));
            if (msg.fields.size() >= 6)
                args.map = move(msg.as<Object>(5));

            auto self = this->shared_from_this();
            for (auto& subKv: kv->second)
                this->post(&ClientImpl::dispatchEvent, self, subKv.second,
                           pubId, args);
        }
    }

    void dispatchEvent(SubscriptionBase::WeakPtr subscription,
                       PublicationId pubId, Args args)
    {
        if (!subscription.expired())
        {
            auto sub = subscription.lock();
            bool insufficientArgs = false;
            bool invalidArgs = false;
            try
            {
                sub->invoke(pubId, args);
            }
            catch (const std::out_of_range&)
            {
                insufficientArgs = true;
            }
            catch (const error::Conversion&)
            {
                invalidArgs = true;
            }
            if (warningHandler_ && (insufficientArgs || invalidArgs))
            {
                std::ostringstream oss;
                if (insufficientArgs)
                    oss << "Received EVENT with insufficient "
                           "positional arguments:\n";
                else
                    oss << "Received EVENT with invalid "
                           "positional argument types:\n";
                oss << "    topic = \"" << sub->topic() << "\"\n"
                       "    subId = " << sub->id() << "\n"
                       "    pubId = " << pubId;
                if (!args.list.empty())
                    oss << "\n    args = " << args.list;
                if (!args.map.empty())
                    oss << "\n    kwargs = " << args.map;
                warn(oss.str());
            }
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
            Args args;
            if (msg.fields.size() >= 5)
                args.list = move(msg.as<Array>(4));
            if (msg.fields.size() >= 6)
                args.map = move(msg.as<Object>(5));

            auto self = this->shared_from_this();
            this->post(&ClientImpl::dispatchInvocation, self, kv->second,
                       requestId, move(args));
        }
        else
        {
            this->sendError(WampMsgType::invocation, requestId,
                            "wamp.error.no_such_procedure",
                            Args{{"The called procedure does not exist"}});
        }
    }

    void dispatchInvocation(RegistrationBase::WeakPtr registration,
                            RequestId requestId, Args args)
    {
        if (!registration.expired())
        {
            auto reg = registration.lock();
            try
            {
                reg->invoke(Invocation(this->shared_from_this(), requestId),
                            std::move(args));
            }
            catch (const std::out_of_range&)
            {
                this->sendError(WampMsgType::invocation, requestId,
                    "wamp.error.invalid_argument",
                    Args{{"Insufficient arguments"}});
            }
            catch (const error::Conversion& e)
            {
                this->sendError(WampMsgType::invocation, requestId,
                    "wamp.error.invalid_argument",
                    Args{{"Argument type mismatch", e.what()}});
            }
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
                    WampErrc defaultErrc, const AsyncHandler<TResult>& handler)
    {
        bool success = checkError(ec, handler);
        if (success)
        {
            if (reply.type == WampMsgType::error)
            {
                success = false;
                WampErrc errc = defaultErrc;
                const auto& uri = reply.as<String>(4);
                lookupWampErrorUri(uri, errc);

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
                   WampErrc defaultErrc)
    {
        auto self = this->shared_from_this();
        checkReply<bool>(type, ec, reply, defaultErrc,
            [this, self](AsyncResult<bool> result)
            {
                if (!result)
                    warn(error::Wamp::makeMessage(result.errorCode(),
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
        Object acked{{"acknowledge", true}};

        publishMsg_            = M{ T::publish,     {n, n, o, s} };
        publishArgsMsg_        = M{ T::publish,     {n, n, o, s, a} };
        publishKvArgsMsg_      = M{ T::publish,     {n, n, o, s, a, o} };
        ackedPublishMsg_       = M{ T::publish,     {n, n, acked, s} };
        ackedPublishArgsMsg_   = M{ T::publish,     {n, n, acked, s, a} };
        ackedPublishKvArgsMsg_ = M{ T::publish,     {n, n, acked, s, a, o} };
        subscribeMsg_          = M{ T::subscribe,   {n, n, o, s} };
        unsubscribeMsg_        = M{ T::unsubscribe, {n, n, n} };
        enrollMsg_             = M{ T::enroll,      {n, n, o, s} };
        unregisterMsg_         = M{ T::unregister,  {n, n, n} };
        callMsg_               = M{ T::call,        {n, n, o, s} };
        callWithArgsMsg_       = M{ T::call,        {n, n, o, s, a} };
        callWithKvArgsMsg_     = M{ T::call,        {n, n, o, s, a, o} };
        yieldMsg_              = M{ T::yield,       {n, n, o} };
        yieldWithArgsMsg_      = M{ T::yield,       {n, n, o, a} };
        yieldWithKvArgsMsg_    = M{ T::yield,       {n, n, o, a, o} };
    }

    Object peerInfo_;
    TopicMap topics_;
    Readership readership_;
    Registry registry_;
    LogHandler warningHandler_;

    Message publishMsg_;
    Message publishArgsMsg_;
    Message publishKvArgsMsg_;
    Message ackedPublishMsg_;
    Message ackedPublishArgsMsg_;
    Message ackedPublishKvArgsMsg_;
    Message subscribeMsg_;
    Message unsubscribeMsg_;
    Message enrollMsg_;
    Message unregisterMsg_;
    Message callMsg_;
    Message callWithArgsMsg_;
    Message callWithKvArgsMsg_;
    Message yieldMsg_;
    Message yieldWithArgsMsg_;
    Message yieldWithKvArgsMsg_;
};

//------------------------------------------------------------------------------
template <typename TTransportPtr>
static ClientImplBase::Ptr createClientImpl(CodecId codecId,
                                            TTransportPtr&& trn)
{
    using Transport = typename TTransportPtr::element_type;
    switch (codecId)
    {
    case CodecId::json:
        return ClientImpl<Json, Transport>::create(std::move(trn));

    case CodecId::msgpack:
        return ClientImpl<Msgpack, Transport>::create(std::move(trn));

    default:
        assert(false && "Unexpected CodecId");
    }
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENTIMPL_HPP
