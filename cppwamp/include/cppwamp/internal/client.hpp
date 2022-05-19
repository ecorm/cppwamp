/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENT_HPP
#define CPPWAMP_INTERNAL_CLIENT_HPP

#include <cassert>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>
#include <boost/asio/post.hpp>
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../unpacker.hpp"
#include "../version.hpp"
#include "clientinterface.hpp"
#include "peer.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Provides the implementation of the wamp::Session class.
//------------------------------------------------------------------------------
template <typename TCodec, typename TTransport>
class Client : public ClientInterface, public Peer<TCodec, TTransport>
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

    virtual void join(Realm&& realm, AsyncTask<SessionInfo>&& handler) override
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
                        move(handler)(SessionInfo({},
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
                        move(handler)(move(result));
                    }
                }
            });
    }

    virtual void authenticate(Authentication&& auth) override
    {
        authenticateMsg_.at(1) = move(auth.signature(PassKey{}));
        authenticateMsg_.at(2) = move(auth.options(PassKey{}));
        this->send(authenticateMsg_);
    }

    virtual void leave(Reason&& reason, AsyncTask<Reason>&& handler) override
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
                    move(handler)(move(reason));
                }
                readership_.clear();
                registry_.clear();
            });
    }

    virtual void disconnect() override
    {
        pendingInvocations_.clear();
        this->close(false);
    }

    virtual void terminate() override
    {
        using Handler = AsyncTask<std::string>;
        setLogHandlers(Handler(), Handler());
        pendingInvocations_.clear();
        this->close(true);
    }

    virtual void subscribe(Topic&& topic, EventSlot&& slot,
                           AsyncTask<Subscription>&& handler) override
    {
        using std::move;
        SubscriptionRecord rec = {move(topic), move(slot), handler.executor()};

        auto kv = topics_.find(rec.topic.uri());
        if (kv == topics_.end())
        {
            subscribeMsg_.at(2) = rec.topic.options();
            subscribeMsg_.at(3) = rec.topic.uri();
            auto self = this->shared_from_this();
            this->request(subscribeMsg_,
                [this, self, rec, handler](std::error_code ec, Message reply)
                {
                    if (checkReply(WampMsgType::subscribed, ec, reply,
                                   SessionErrc::subscribeError, handler))
                    {
                        auto subId = reply.to<SubscriptionId>(2);
                        auto slotId = nextSlotId();
                        Subscription sub(self, subId, slotId, {});
                        topics_.emplace(rec.topic.uri(), subId);
                        readership_[subId][slotId] = move(rec);
                        std::move(handler)(std::move(sub));
                    }
                });
        }
        else
        {
            auto subId = kv->second;
            auto slotId = nextSlotId();
            Subscription sub{this->shared_from_this(), subId, slotId, {}};
            readership_[subId][slotId] = move(rec);
            std::move(handler)(move(sub));
        }
    }

    virtual void unsubscribe(const Subscription& sub) override
    {
        auto kv = readership_.find(sub.id());
        if (kv != readership_.end())
        {
            auto& localSubs = kv->second;
            if (!localSubs.empty())
            {
                auto subKv = localSubs.find(sub.slotId({}));
                if (subKv != localSubs.end())
                {
                    if (localSubs.size() == 1u)
                        topics_.erase(subKv->second.topic.uri());

                    localSubs.erase(subKv);
                    if (localSubs.empty())
                    {
                        readership_.erase(kv);
                        sendUnsubscribe(sub.id());
                    }
                }
            }
        }
    }

    virtual void unsubscribe(const Subscription& sub,
                             AsyncTask<bool>&& handler) override
    {
        bool unsubscribed = false;
        auto kv = readership_.find(sub.id());
        if (kv != readership_.end())
        {
            auto& localSubs = kv->second;
            if (!localSubs.empty())
            {
                auto subKv = localSubs.find(sub.slotId({}));
                if (subKv != localSubs.end())
                {
                    unsubscribed = true;
                    if (localSubs.size() == 1u)
                        topics_.erase(subKv->second.topic.uri());

                    localSubs.erase(subKv);
                    if (localSubs.empty())
                    {
                        readership_.erase(kv);
                        sendUnsubscribe(sub.id(), std::move(handler));
                        handler = AsyncTask<bool>();
                    }
                }
            }
        }

        if (handler)
            std::move(handler)(unsubscribed);
    }

    virtual void publish(Pub&& pub) override
    {
        this->send(marshallPublish(std::move(pub)));
    }

    virtual void publish(Pub&& pub, AsyncTask<PublicationId>&& handler) override
    {
        pub.options({}).emplace("acknowledge", true);
        auto self = this->shared_from_this();
        this->request(marshallPublish(std::move(pub)),
            [this, self, handler](std::error_code ec, Message reply)
            {
                if (checkReply(WampMsgType::published, ec, reply,
                               SessionErrc::publishError, handler))
                {
                    std::move(handler)(reply.to<PublicationId>(2));
                }
            });
    }

    virtual void enroll(Procedure&& procedure, CallSlot&& callSlot,
                        InterruptSlot&& interruptSlot,
                        AsyncTask<Registration>&& handler) override
    {
        using std::move;
        RegistrationRecord rec{ move(procedure), move(callSlot),
                                move(interruptSlot), handler.executor() };
        enrollMsg_.at(2) = rec.procedure.options();
        enrollMsg_.at(3) = rec.procedure.uri();
        auto self = this->shared_from_this();
        this->request(enrollMsg_,
            [this, self, rec, handler](std::error_code ec, Message reply)
            {
                if (checkReply(WampMsgType::registered, ec, reply,
                               SessionErrc::registerError, handler))
                {
                    auto regId = reply.to<RegistrationId>(2);
                    Registration reg(self, regId, {});
                    registry_[regId] = move(rec);
                    move(handler)(move(reg));
                }
            });
    }

    virtual void unregister(const Registration& reg) override
    {
        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            if (state() == State::established)
            {
                unregisterMsg_.at(2) = reg.id();
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

    virtual void unregister(const Registration& reg,
                            AsyncTask<bool>&& handler) override
    {
        CPPWAMP_LOGIC_CHECK(state() == State::established,
                            "Session is not established");
        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            unregisterMsg_.at(2) = reg.id();
            auto self = this->shared_from_this();
            this->request( unregisterMsg_,
                [this, self, handler](std::error_code ec, Message reply)
                {
                    if (checkReply(WampMsgType::unregistered, ec, reply,
                                   SessionErrc::unregisterError, handler))
                        std::move(handler)(true);
                });
        }
        else
            std::move(handler)(false);
    }

    virtual RequestId call(Rpc&& rpc, AsyncTask<Result>&& handler) override
    {
        using std::move;
        Error* errorPtr = rpc.error({});
        RequestId id = 0;

        auto opts = RequestOptions().withProgressiveResponse(
                        rpc.progressiveResultsAreEnabled());

        if (!rpc.kwargs().empty())
        {
            callWithKwargsMsg_.at(2) = move(rpc.options({}));
            callWithKwargsMsg_.at(3) = move(rpc.procedure({}));
            callWithKwargsMsg_.at(4) = move(rpc.args({}));
            callWithKwargsMsg_.at(5) = move(rpc.kwargs({}));
            id = callProcedure(callWithKwargsMsg_, errorPtr, move(handler),
                               opts);
        }
        else if (!rpc.args().empty())
        {
            callWithArgsMsg_.at(2) = move(rpc.options({}));
            callWithArgsMsg_.at(3) = move(rpc.procedure({}));
            callWithArgsMsg_.at(4) = move(rpc.args({}));
            id = callProcedure(callWithArgsMsg_, errorPtr, move(handler),
                               opts);
        }
        else
        {
            callMsg_.at(2) = move(rpc.options({}));
            callMsg_.at(3) = move(rpc.procedure({}));
            id = callProcedure(callMsg_, errorPtr, move(handler), opts);
        }

        return id;
    }

    virtual void cancel(Cancellation&& cancellation) override
    {
        cancelMsg_.at(1) = cancellation.requestId();
        cancelMsg_.at(2) = move(cancellation.options({}));
        this->send(cancelMsg_);
    }

    virtual void yield(RequestId reqId, Result&& result) override
    {
        if (!result.isProgressive())
            pendingInvocations_.erase(reqId);

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
        pendingInvocations_.erase(reqId);
        this->sendError(WampMsgType::invocation, reqId, std::move(failure));
    }

    virtual void setLogHandlers(AsyncTask<std::string> warningHandler,
                                AsyncTask<std::string> traceHandler) override
    {
        warningHandler_ = std::move(warningHandler);
        this->setTraceHandler(std::move(traceHandler));
    }

    virtual void setChallengeHandler(AsyncTask<Challenge> handler) override
    {
        challengeHandler_ = std::move(handler);
    }

private:
    struct SubscriptionRecord
    {
        using Slot = std::function<void (Event)>;

        SubscriptionRecord() : topic("") {}

        SubscriptionRecord(Topic&& topic, Slot&& slot, AnyExecutor exec)
            : topic(std::move(topic)), slot(std::move(slot)), executor(exec)
        {}

        Topic topic;
        Slot slot;
        AnyExecutor executor;
    };

    struct RegistrationRecord
    {
        using CallSlot = std::function<Outcome (Invocation)>;
        using InterruptSlot = std::function<Outcome (Interruption)>;

        RegistrationRecord() : procedure("") {}

        RegistrationRecord(Procedure&& procedure, CallSlot&& callSlot,
                           InterruptSlot&& interruptSlot, AnyExecutor exec)
            : procedure(std::move(procedure)),
              callSlot(std::move(callSlot)),
              interruptSlot(std::move(interruptSlot)),
              executor(exec)
        {}

        Procedure procedure;
        CallSlot callSlot;
        InterruptSlot interruptSlot;
        AnyExecutor executor;
    };

    using Base           = Peer<Codec, Transport>;
    using RequestOptions = typename Base::RequestOptions;
    using WampMsgType    = internal::WampMsgType;
    using Message        = internal::WampMessage;
    using SlotId         = uint64_t;
    using LocalSubs      = std::map<SlotId, SubscriptionRecord>;
    using Readership     = std::map<SubscriptionId, LocalSubs>;
    using TopicMap       = std::map<std::string, SubscriptionId>;
    using Registry       = std::map<RegistrationId, RegistrationRecord>;
    using InvocationMap  = std::map<RequestId, RegistrationId>;

    Client(TransportPtr transport)
        : Base(std::move(transport))
    {
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

    void sendUnsubscribe(SubscriptionId subId, AsyncTask<bool>&& handler)
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
                    std::move(handler)(true);
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

    RequestId callProcedure(Message& msg, Error* errorPtr,
                            AsyncTask<Result>&& handler, RequestOptions opts)
    {
        auto self = this->shared_from_this();
        return this->request(msg, opts,
            [this, self, errorPtr, handler](std::error_code ec, Message reply)
            {
                if ((reply.type == WampMsgType::error) && (errorPtr != nullptr))
                {
                    *errorPtr = Error(reply.as<String>(4));
                    if (reply.size() >= 6)
                        errorPtr->withArgList(reply.as<Array>(5));
                    if (reply.size() >= 7)
                        errorPtr->withKwargs(reply.as<Object>(6));
                }

                if (checkReply(WampMsgType::result, ec, reply,
                               SessionErrc::callError, handler))
                {
                    using std::move;
                    Result result({}, reply.to<RequestId>(1),
                                  move(reply.as<Object>(2)));

                    if (reply.size() >= 4)
                        result.withArgList(move(reply.as<Array>(3)));
                    if (reply.size() >= 5)
                        result.withKwargs(move(reply.as<Object>(4)));
                    move(handler)(move(result));
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
        case WampMsgType::challenge:
            onChallenge(std::move(msg));
            break;

        case WampMsgType::event:
            onEvent(std::move(msg));
            break;

        case WampMsgType::invocation:
            onInvocation(std::move(msg));
            break;

        case WampMsgType::interrupt:
            onInterrupt(std::move(msg));
            break;

        default:
            assert(false);
        }
    }

    void onChallenge(Message&& msg)
    {
        using std::move;

        auto self = this->shared_from_this();
        Challenge challenge({}, self, std::move(msg.as<String>(1)));
        challenge.withOptions(std::move(msg.as<Object>(2)));

        if (challengeHandler_)
        {
            challengeHandler_(std::move(challenge));
        }
        else
        {
            if (warningHandler_)
            {
                std::ostringstream oss;
                oss << "Received a CHALLENGE with no registered handler "
                       "(with method=" << challenge.method() << " extra="
                    << challenge.options() << ")";
                warn(oss.str());
            }

            // Send empty signature to avoid deadlock with other peer.
            authenticate(Authentication(""));
        }
    }

    void onEvent(Message&& msg)
    {
        using std::move;

        auto subId = msg.to<SubscriptionId>(1);
        auto pubId = msg.to<PublicationId>(2);

        auto kv = readership_.find(subId);
        if (kv != readership_.end())
        {
            const auto& localSubs = kv->second;
            assert(!localSubs.empty());
            Event event({},
                        msg.to<SubscriptionId>(1),
                        msg.to<PublicationId>(2),
                        localSubs.begin()->second.executor,
                        move(msg.as<Object>(3)));

            if (msg.fields.size() >= 5)
                event.args({}) = move(msg.as<Array>(4));
            if (msg.fields.size() >= 6)
                event.kwargs({}) = move(msg.as<Object>(5));

            auto self = this->shared_from_this();
            for (const auto& subKv: localSubs)
                dispatchEvent(subKv.second, event);
        }
        else if (warningHandler_)
        {
            std::ostringstream oss;
            oss << "Received an EVENT that is not subscribed to "
                   "(with subId=" << subId << " pubId=" << pubId << ")";
            warn(oss.str());
        }
    }

    void dispatchEvent(const SubscriptionRecord& sub, const Event& event)
    {
        auto self = this->shared_from_this();
        const auto& slot = sub.slot;
        boost::asio::post(sub.executor, [this, self, slot, event]()
        {
            // Copy the subscription and publication IDs before the Event
            // object gets moved away.
            auto subId = event.subId();
            auto pubId = event.pubId();

            /*  The catch clauses are to prevent the publisher crashing
                subscribers when it passes arguments having incorrect type. */
            try
            {
                slot(std::move(event));
            }
            catch (const Error& e)
            {
                if (warningHandler_)
                    warnEventError(e, subId, pubId);
            }
            catch (const error::BadType& e)
            {
                if (warningHandler_)
                    warnEventError(Error(e), subId, pubId);
            }
        });
    }

    void warnEventError(const Error& e, SubscriptionId subId,
                        PublicationId pubId)
    {
        std::ostringstream oss;
        oss << "EVENT handler reported an error: "
            << e.args()
            << " (with subId=" << subId
            << " pubId=" << pubId << ")";
        warn(oss.str());
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
            const RegistrationRecord& rec = kv->second;
            Invocation inv({}, self, requestId, rec.executor,
                           move(msg.as<Object>(3)));
            if (msg.fields.size() >= 5)
                inv.args({}) = move(msg.as<Array>(4));
            if (msg.fields.size() >= 6)
                inv.kwargs({}) = move(msg.as<Object>(5));

            pendingInvocations_[requestId] = regId;
            dispatchRpcRequest(rec.executor, rec.callSlot, inv);
        }
        else
        {
            this->sendError(WampMsgType::invocation, requestId,
                    Error("wamp.error.no_such_procedure")
                        .withArgs("There is no RPC with registration ID " +
                                  std::to_string(regId)));
        }
    }

    void onInterrupt(Message&& msg)
    {
        using std::move;

        auto requestId = msg.to<RequestId>(1);

        auto found = pendingInvocations_.find(requestId);
        if (found != pendingInvocations_.end())
        {
            auto registrationId = found->second;
            pendingInvocations_.erase(found);
            auto kv = registry_.find(registrationId);
            if ((kv != registry_.end()) &&
                (kv->second.interruptSlot != nullptr))
            {
                auto self = this->shared_from_this();
                const RegistrationRecord& rec = kv->second;
                Interruption intr({}, self, requestId, rec.executor,
                                  move(msg.as<Object>(2)));
                dispatchRpcRequest(rec.executor, rec.interruptSlot, intr);
            }
        }
    }

    template <typename TSlot, typename TRequest>
    void dispatchRpcRequest(AnyExecutor exec, const TSlot& slot,
                            const TRequest& request)
    {
        auto self = this->shared_from_this();
        boost::asio::post(exec, [this, self, slot, request]()
        {
            // Copy the request ID before the request object gets moved away.
            auto requestId = request.requestId();

            try
            {
                Outcome outcome(slot(std::move(request)));
                switch (outcome.type())
                {
                case Outcome::Type::deferred:
                    // Do nothing
                    break;

                case Outcome::Type::result:
                    yield(requestId, std::move(outcome).asResult());
                    break;

                case Outcome::Type::error:
                    yield(requestId, std::move(outcome).asError());
                    break;

                default:
                    assert(false && "unexpected Outcome::Type");
                }
            }
            catch (Error& error)
            {
                yield(requestId, std::move(error));
            }
            catch (const error::BadType& e)
            {
                // Forward Variant conversion exceptions as ERROR messages.
                yield(requestId, Error(e));
            }
        });
    }

    template <typename THandler>
    bool checkError(std::error_code ec, THandler&& handler)
    {
        if (ec)
            std::forward<THandler>(handler)(ec);
        return !ec;
    }

    template <typename THandler>
    bool checkReply(WampMsgType type, std::error_code ec, const Message& reply,
                    SessionErrc defaultErrc, THandler&& handler)
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

                using ResultType = typename ResultTypeOfHandler<
                        typename std::decay<THandler>::type >::Type;
                ResultType result(make_error_code(errc), oss.str());
                std::forward<THandler>(handler)(std::move(result));
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
        checkReply(type, ec, reply, defaultErrc, AsyncHandler<bool>(
            [this, self](AsyncResult<bool> result)
            {
                if (!result)
                    warn(error::Failure::makeMessage(result.errorCode(),
                                                     result.errorInfo()));
            }));
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

        authenticateMsg_       = M{ T::authenticate, {n, s, o} };
        publishMsg_            = M{ T::publish,      {n, n, o, s} };
        publishArgsMsg_        = M{ T::publish,      {n, n, o, s, a} };
        publishKwargsMsg_      = M{ T::publish,      {n, n, o, s, a, o} };
        subscribeMsg_          = M{ T::subscribe,    {n, n, o, s} };
        unsubscribeMsg_        = M{ T::unsubscribe,  {n, n, n} };
        enrollMsg_             = M{ T::enroll,       {n, n, o, s} };
        unregisterMsg_         = M{ T::unregister,   {n, n, n} };
        callMsg_               = M{ T::call,         {n, n, o, s} };
        callWithArgsMsg_       = M{ T::call,         {n, n, o, s, a} };
        callWithKwargsMsg_     = M{ T::call,         {n, n, o, s, a, o} };
        cancelMsg_             = M{ T::cancel,       {n, n, o} };
        yieldMsg_              = M{ T::yield,        {n, n, o} };
        yieldWithArgsMsg_      = M{ T::yield,        {n, n, o, a} };
        yieldWithKwargsMsg_    = M{ T::yield,        {n, n, o, a, o} };
    }

    SlotId nextSlotId() {return nextSlotId_++;}

    SlotId nextSlotId_ = 0;
    TopicMap topics_;
    Readership readership_;
    Registry registry_;
    InvocationMap pendingInvocations_;
    AsyncTask<std::string> warningHandler_;
    AsyncTask<Challenge> challengeHandler_;

    Message authenticateMsg_;
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
    Message cancelMsg_;
    Message yieldMsg_;
    Message yieldWithArgsMsg_;
    Message yieldWithKwargsMsg_;
};


} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
