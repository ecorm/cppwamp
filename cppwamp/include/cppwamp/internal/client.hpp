/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENT_HPP
#define CPPWAMP_INTERNAL_CLIENT_HPP

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <boost/asio/post.hpp>
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../version.hpp"
#include "callertimeout.hpp"
#include "clientinterface.hpp"
#include "config.hpp"
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
        this->start();

        auto self = this->shared_from_this();
        this->request(realm.message({}),
            [this, self, handler, realmUri]
                      (std::error_code ec, Message reply) mutable
            {
                if (checkError(ec, handler))
                {
                    if (reply.type() == WampMsgType::welcome)
                        onWelcome(move(handler), move(reply), move(realmUri));
                    else
                        onJoinAborted(move(handler), reply);
                }
            });
    }

    virtual void authenticate(Authentication&& auth) override
    {
        this->send(auth.message({}));
    }

    virtual void leave(Reason&& reason, AsyncTask<Reason>&& handler) override
    {
        using std::move;
        timeoutScheduler_->clear();
        auto self = this->shared_from_this();
        Base::adjourn(
            reason,
            [this, self, CPPWAMP_MVCAP(handler)](std::error_code ec,
                                                 Message reply)
            {
                if (checkError(ec, handler))
                {
                    auto& goodBye = message_cast<GoodbyeMessage>(reply);
                    move(handler)(Reason({}, std::move(goodBye)));
                }
                readership_.clear();
                registry_.clear();
            });
    }

    virtual void disconnect() override
    {
        pendingInvocations_.clear();
        timeoutScheduler_->clear();
        this->close(false);
    }

    virtual void terminate() override
    {
        setSessionHandlers({}, {}, {}, {});
        pendingInvocations_.clear();
        timeoutScheduler_->clear();
        this->close(true);
    }

    virtual void subscribe(Topic&& topic, EventSlot&& slot,
                           AsyncTask<Subscription>&& handler) override
    {
        using std::move;
        SubscriptionRecord rec = {topic.uri(), move(slot), handler.executor()};

        auto kv = topics_.find(rec.topicUri);
        if (kv == topics_.end())
        {
            auto self = this->shared_from_this();
            this->request(
                topic.message({}),
                [this, self, CPPWAMP_MVCAP(rec), CPPWAMP_MVCAP(handler)]
                (std::error_code ec, Message reply)
                {
                    if (checkReply(WampMsgType::subscribed, ec, reply,
                                   SessionErrc::subscribeError, handler))
                    {
                        const auto& subMsg =
                            message_cast<SubscribedMessage>(reply);
                        auto subId = subMsg.subscriptionId();
                        auto slotId = nextSlotId();
                        Subscription sub(self, subId, slotId, {});
                        topics_.emplace(rec.topicUri, subId);
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
                        topics_.erase(subKv->second.topicUri);

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
                        topics_.erase(subKv->second.topicUri);

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
        this->send(pub.message({}));
    }

    virtual void publish(Pub&& pub, AsyncTask<PublicationId>&& handler) override
    {
        pub.withOption("acknowledge", true);
        auto self = this->shared_from_this();
        this->request(
            pub.message({}),
            [this, self, CPPWAMP_MVCAP(handler)](std::error_code ec,
                                                 Message reply)
            {
                if (checkReply(WampMsgType::published, ec, reply,
                               SessionErrc::publishError, handler))
                {
                    const auto& pubMsg = message_cast<PublishedMessage>(reply);
                    std::move(handler)(pubMsg.publicationId());
                }
            });
    }

    virtual void enroll(Procedure&& procedure, CallSlot&& callSlot,
                        InterruptSlot&& interruptSlot,
                        AsyncTask<Registration>&& handler) override
    {
        using std::move;
        RegistrationRecord rec{ move(callSlot), move(interruptSlot),
                                handler.executor() };
        auto self = this->shared_from_this();
        this->request(
            procedure.message({}),
            [this, self, CPPWAMP_MVCAP(rec), CPPWAMP_MVCAP(handler)]
            (std::error_code ec, Message reply)
            {
                if (checkReply(WampMsgType::registered, ec, reply,
                               SessionErrc::registerError, handler))
                {
                    const auto& regMsg = message_cast<RegisteredMessage>(reply);
                    auto regId = regMsg.registrationId();
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
                auto self = this->shared_from_this();
                UnregisterMessage msg(reg.id());
                this->request( msg,
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
            auto self = this->shared_from_this();
            UnregisterMessage msg(reg.id());
            this->request(
                msg,
                [this, self, CPPWAMP_MVCAP(handler)](std::error_code ec,
                                                     Message reply)
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
        auto opts = RequestOptions().withProgressiveResponse(
                        rpc.progressiveResultsAreEnabled());
        auto self = this->shared_from_this();

        auto requestId = this->request(rpc.message({}), opts,
            [this, self, errorPtr, CPPWAMP_MVCAP(handler)](std::error_code ec,
                                                           Message reply)
            {
                if (checkReply(WampMsgType::result, ec, reply,
                               SessionErrc::callError, handler, errorPtr))
                {
                    auto& resultMsg = message_cast<ResultMessage>(reply);
                    move(handler)(Result({}, std::move(resultMsg)));
                }
            });

        if (rpc.callerTimeout().count() != 0)
            timeoutScheduler_->add(rpc.callerTimeout(), requestId);

        return requestId;
    }

    virtual void cancel(Cancellation&& cancellation) override
    {
        this->cancelCall(cancellation);
    }

    virtual void yield(RequestId reqId, Result&& result) override
    {
        if (!result.isProgressive())
            pendingInvocations_.erase(reqId);
        this->send(result.yieldMessage({}, reqId));
    }

    virtual void yield(RequestId reqId, Error&& error) override
    {
        pendingInvocations_.erase(reqId);
        this->sendError(WampMsgType::invocation, reqId, std::move(error));
    }

    virtual void setSessionHandlers(
        AsyncTask<std::string> warningHandler,
        AsyncTask<std::string> traceHandler,
        AsyncTask<SessionState> stateChangeHandler,
        AsyncTask<Challenge> challengeHandler) override
    {
        warningHandler_ = std::move(warningHandler);
        this->setTraceHandler(std::move(traceHandler));
        this->setStateChangeHandler(std::move(stateChangeHandler));
        challengeHandler_ = std::move(challengeHandler);
    }

private:
    struct SubscriptionRecord
    {
        using Slot = std::function<void (Event)>;

        String topicUri;
        Slot slot;
        AnyExecutor executor;
    };

    struct RegistrationRecord
    {
        using CallSlot = std::function<Outcome (Invocation)>;
        using InterruptSlot = std::function<Outcome (Interruption)>;

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
    using CallerTimeoutDuration = typename Rpc::CallerTimeoutDuration;

    Client(TransportPtr transport)
        : Base(std::move(transport)),
          timeoutScheduler_(CallerTimeoutScheduler::create(this->executor()))
    {}

    Ptr shared_from_this()
    {
        return std::static_pointer_cast<Client>( Base::shared_from_this() );
    }

    void sendUnsubscribe(SubscriptionId subId)
    {
        if (state() == State::established)
        {
            auto self = this->shared_from_this();
            UnsubscribeMessage msg(subId);
            this->request(
                msg,
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
        auto self = this->shared_from_this();
        UnsubscribeMessage msg(subId);
        this->request(
            msg,
            [this, self, CPPWAMP_MVCAP(handler)](std::error_code ec,
                                                 Message reply)
            {
                if (checkReply(WampMsgType::unsubscribed, ec, reply,
                               SessionErrc::unsubscribeError, handler))
                    std::move(handler)(true);
            });
    }

    virtual bool isMsgSupported(const MessageTraits& traits) override
    {
        return traits.isClientRx;
    }

    virtual void onInbound(Message msg) override
    {
        switch (msg.type())
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

    void onWelcome(AsyncTask<SessionInfo>&& handler, Message&& reply,
                   String&& realmUri)
    {
        WeakPtr self = this->shared_from_this();
        timeoutScheduler_->listen([self](RequestId reqId)
        {
            auto ptr = self.lock();
            if (ptr)
                ptr->cancel(Cancellation(reqId, CancelMode::killNoWait));
        });

        using std::move;
        auto& welcomeMsg = message_cast<WelcomeMessage>(reply);
        move(handler)(SessionInfo({}, move(realmUri), move(welcomeMsg)));
    }

    void onJoinAborted(AsyncTask<SessionInfo>&& handler, const Message& reply)
    {
        const auto& uri = message_cast<AbortMessage>(reply).reasonUri();
        auto errc = lookupWampErrorUri(uri, SessionErrc::joinError);

        std::ostringstream oss;
        oss << "with URI=" << uri;
        if (!reply.as<Object>(1).empty())
            oss << ", Details=" << reply.at(1);

        AsyncResult<SessionInfo> result(make_error_code(errc),
                                        oss.str());
        std::move(handler)(std::move(result));
    }

    void onChallenge(Message&& msg)
    {
        using std::move;

        auto self = this->shared_from_this();
        auto& challengeMsg = message_cast<ChallengeMessage>(msg);
        Challenge challenge({}, self, std::move(challengeMsg));

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
        auto& eventMsg = message_cast<EventMessage>(msg);
        auto kv = readership_.find(eventMsg.subscriptionId());
        if (kv != readership_.end())
        {
            const auto& localSubs = kv->second;
            assert(!localSubs.empty());
            Event event({}, localSubs.begin()->second.executor,
                        std::move(eventMsg));
            for (const auto& subKv: localSubs)
                dispatchEvent(subKv.second, event);
        }
        else if (warningHandler_)
        {
            std::ostringstream oss;
            oss << "Received an EVENT that is not subscribed to "
                   "(with subId=" << eventMsg.subscriptionId()
                << " pubId=" << eventMsg.publicationId() << ")";
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
        auto& invMsg = message_cast<InvocationMessage>(msg);
        auto requestId = invMsg.requestId();
        auto regId = invMsg.registrationId();

        auto kv = registry_.find(regId);
        if (kv != registry_.end())
        {
            auto self = this->shared_from_this();
            const RegistrationRecord& rec = kv->second;
            Invocation inv({}, self, rec.executor, std::move(invMsg));
            pendingInvocations_[requestId] = regId;
            dispatchRpcRequest(rec.executor, rec.callSlot, std::move(inv));
        }
        else
        {
            this->sendError(WampMsgType::invocation, requestId,
                            Error("wamp.error.no_such_procedure"));
        }
    }

    void onInterrupt(Message&& msg)
    {
        auto& interruptMsg = message_cast<InterruptMessage>(msg);
        auto found = pendingInvocations_.find(interruptMsg.requestId());
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
                Interruption intr({}, self, rec.executor,
                                  std::move(interruptMsg));
                dispatchRpcRequest(rec.executor, rec.interruptSlot,
                                   std::move(intr));
            }
        }
    }

    template <typename TSlot, typename TRequest>
    void dispatchRpcRequest(AnyExecutor exec, const TSlot& slot,
                            TRequest&& request)
    {
        auto self = this->shared_from_this();
        boost::asio::post(exec, [this, self, slot, CPPWAMP_MVCAP(request)]()
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
    bool checkReply(WampMsgType type, std::error_code ec, Message& reply,
                    SessionErrc defaultErrc, THandler&& handler,
                    Error* errorPtr = nullptr)
    {
        bool success = checkError(ec, handler);
        if (success)
        {
            if (reply.type() == WampMsgType::error)
            {
                success = false;
                auto& errMsg = message_cast<ErrorMessage>(reply);
                const auto& uri = errMsg.reasonUri();
                auto errc = lookupWampErrorUri(uri, defaultErrc);

                std::ostringstream oss;
                oss << "with URI=" << uri;
                if (!errMsg.args().empty())
                    oss << ", Args=" << errMsg.args();
                if (!errMsg.kwargs().empty())
                    oss << ", ArgsKv=" << errMsg.kwargs();

                if (errorPtr != nullptr)
                    *errorPtr = Error({}, std::move(errMsg));

                using ResultType = typename ResultTypeOfHandler<
                        typename std::decay<THandler>::type >::Type;
                ResultType result(make_error_code(errc), oss.str());
                std::forward<THandler>(handler)(std::move(result));
            }
            else
                assert((reply.type() == type) && "Unexpected WAMP message type");
        }
        return success;
    }

    void warnReply(WampMsgType type, std::error_code ec, Message& reply,
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

    SlotId nextSlotId() {return nextSlotId_++;}

    SlotId nextSlotId_ = 0;
    TopicMap topics_;
    Readership readership_;
    Registry registry_;
    InvocationMap pendingInvocations_;
    CallerTimeoutScheduler::Ptr timeoutScheduler_;
    AsyncTask<std::string> warningHandler_;
    AsyncTask<Challenge> challengeHandler_;
};


} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
