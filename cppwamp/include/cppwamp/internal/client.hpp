/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENT_HPP
#define CPPWAMP_INTERNAL_CLIENT_HPP

#include <cassert>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <boost/asio/post.hpp>
#include "../anyhandler.hpp"
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../version.hpp"
#include "callertimeout.hpp"
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

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(TransportPtr&& transport)
    {
        return Ptr(new Client(std::move(transport)));
    }

    ~Client() override {terminate();}

    State state() const override {return Base::state();}

    IoStrand strand() const override {return Base::strand();}

    void join(Realm&& realm, CompletionHandler<SessionInfo>&& handler) override
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<SessionInfo> handler;
            String realmUri;
            Abort* abortPtr;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkError(ec, handler))
                {
                    if (reply.type() == WampMsgType::welcome)
                        me.onWelcome(std::move(handler), std::move(reply),
                                     std::move(realmUri));
                    else
                        me.onJoinAborted(std::move(handler), std::move(reply),
                                         abortPtr);
                }
            }
        };

        realm.withOption("agent", Version::agentString())
             .withOption("roles", roles());
        this->start();
        this->request(realm.message({}),
                      Requested{shared_from_this(), std::move(handler),
                                realm.uri(), realm.abort({})});
    }

    void authenticate(Authentication&& auth) override
    {
        this->send(auth.message({}));
    }

    void safeAuthenticate(Authentication&& auth) override
    {
        struct Dispatched
        {
            std::weak_ptr<Client> self;
            Authentication auth;

            void operator()()
            {
                auto me = self.lock();
                if (me)
                    me->authenticate(std::move(auth));
            }
        };

        boost::asio::dispatch(strand(),
                              Dispatched{shared_from_this(), std::move(auth)});
    }

    void leave(Reason&& reason, CompletionHandler<Reason>&& handler) override
    {
        struct Adjourned
        {
            Ptr self;
            CompletionHandler<Reason> handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                me.readership_.clear();
                me.registry_.clear();
                if (me.checkError(ec, handler))
                {
                    auto& goodBye = message_cast<GoodbyeMessage>(reply);
                    me.dispatchUserHandler(handler,
                                           Reason({}, std::move(goodBye)));
                }
            }
        };

        using std::move;
        timeoutScheduler_->clear();
        auto self = this->shared_from_this();
        Base::adjourn(reason,
                      Adjourned{shared_from_this(), std::move(handler)});
    }

    void disconnect() override
    {
        pendingInvocations_.clear();
        timeoutScheduler_->clear();
        this->close(false);
    }

    void terminate() override
    {
        initialize({}, {}, {}, {}, {});
        pendingInvocations_.clear();
        timeoutScheduler_->clear();
        this->close(true);
    }

    void subscribe(Topic&& topic, EventSlot&& slot,
                   CompletionHandler<Subscription>&& handler) override
    {
        struct Requested
        {
            Ptr self;
            SubscriptionRecord rec;
            CompletionHandler<Subscription> handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkReply(WampMsgType::subscribed, ec, reply,
                                  SessionErrc::subscribeError, handler))
                {
                    const auto& subMsg = message_cast<SubscribedMessage>(reply);
                    auto subId = subMsg.subscriptionId();
                    auto slotId = me.nextSlotId();
                    Subscription sub(self, subId, slotId, {});
                    me.topics_.emplace(rec.topicUri, subId);
                    me.readership_[subId][slotId] = std::move(rec);
                    me.dispatchUserHandler(handler, std::move(sub));
                }

            }
        };

        using std::move;
        SubscriptionRecord rec = {topic.uri(), move(slot)};

        auto kv = topics_.find(rec.topicUri);
        if (kv == topics_.end())
        {
            auto self = this->shared_from_this();
            this->request(
                topic.message({}),
                Requested{shared_from_this(), move(rec), move(handler)});
        }
        else
        {
            auto subId = kv->second;
            auto slotId = nextSlotId();
            Subscription sub{this->shared_from_this(), subId, slotId, {}};
            readership_[subId][slotId] = move(rec);
            dispatchUserHandler(handler, move(sub));
        }
    }

    void unsubscribe(const Subscription& sub) override
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

    void unsubscribe(const Subscription& sub,
                     CompletionHandler<bool>&& handler) override
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
                    }
                }
            }
        }
        else
        {
            postVia(userExecutor(), std::move(handler), unsubscribed);
        }
    }

    void safeUnsubscribe(const Subscription& sub) override
    {
        auto self = std::weak_ptr<Client>(shared_from_this());
        boost::asio::dispatch(
            strand(),
            [self, sub]() mutable
            {
                auto me = self.lock();
                if (me)
                    me->unsubscribe(std::move(sub));
            });
    }

    void publish(Pub&& pub) override
    {
        this->send(pub.message({}));
    }

    void publish(Pub&& pub, CompletionHandler<PublicationId>&& handler) override
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<PublicationId> handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkReply(WampMsgType::published, ec, reply,
                                  SessionErrc::publishError, handler))
                {
                    const auto& pubMsg = message_cast<PublishedMessage>(reply);
                    me.dispatchUserHandler(handler, pubMsg.publicationId());
                }
            }
        };

        pub.withOption("acknowledge", true);
        auto self = this->shared_from_this();
        this->request(pub.message({}),
                      Requested{shared_from_this(), std::move(handler)});
    }

    void enroll(Procedure&& procedure, CallSlot&& callSlot,
                InterruptSlot&& interruptSlot,
                CompletionHandler<Registration>&& handler) override
    {
        struct Requested
        {
            Ptr self;
            RegistrationRecord rec;
            CompletionHandler<Registration> handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkReply(WampMsgType::registered, ec, reply,
                                  SessionErrc::registerError, handler))
                {
                    const auto& regMsg = message_cast<RegisteredMessage>(reply);
                    auto regId = regMsg.registrationId();
                    Registration reg(self, regId, {});
                    me.registry_[regId] = std::move(rec);
                    me.dispatchUserHandler(handler, std::move(reg));
                }
            }
        };

        using std::move;
        RegistrationRecord rec{ move(callSlot), move(interruptSlot) };
        auto self = this->shared_from_this();
        this->request(procedure.message({}),
                      Requested{shared_from_this(), move(rec), move(handler)});
    }

    void unregister(const Registration& reg) override
    {
        struct Requested
        {
            Ptr self;

            void operator()(std::error_code ec, Message reply)
            {
                // Don't propagate WAMP errors, as we prefer this
                // to be a no-fail cleanup operation.
                self->warnReply(WampMsgType::unregistered, ec, reply,
                                SessionErrc::unregisterError);
            }
        };

        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            if (state() == State::established)
            {
                auto self = this->shared_from_this();
                UnregisterMessage msg(reg.id());
                this->request(msg, Requested{shared_from_this()});
            }
        }
    }

    void unregister(const Registration& reg,
                    CompletionHandler<bool>&& handler) override
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<bool> handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkReply(WampMsgType::unregistered, ec, reply,
                                  SessionErrc::unregisterError, handler))
                {
                    me.dispatchUserHandler(handler, true);
                }
            }
        };

        CPPWAMP_LOGIC_CHECK(state() == State::established,
                            "Session is not established");
        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            auto self = this->shared_from_this();
            UnregisterMessage msg(reg.id());
            this->request(msg,
                          Requested{shared_from_this(), std::move(handler)});
        }
        else
        {
            postVia(userExecutor(), std::move(handler), false);
        }
    }

    void safeUnregister(const Registration& reg) override
    {
        auto self = std::weak_ptr<Client>(shared_from_this());
        boost::asio::dispatch(
            strand(),
            [self, reg]() mutable
            {
                auto me = self.lock();
                if (me)
                    me->unregister(std::move(reg));
            });
    }

    CallChit oneShotCall(Rpc&& rpc,
                         CompletionHandler<Result>&& handler) override
    {
        struct Requested
        {
            Ptr self;
            Error* errorPtr;
            CompletionHandler<Result> handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkReply(WampMsgType::result, ec, reply,
                                  SessionErrc::callError, handler, errorPtr))
                {
                    auto& resultMsg = message_cast<ResultMessage>(reply);
                    me.dispatchUserHandler(handler,
                                           Result({}, std::move(resultMsg)));
                }
            }
        };

        auto self = this->shared_from_this();
        auto cancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = this->request(
            rpc.message({}),
            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
        CallChit chit{shared_from_this(), requestId, rpc.cancelMode(), {}};

        if (cancelSlot.is_connected())
        {
            cancelSlot.assign(
                [chit](boost::asio::cancellation_type_t) {chit.cancel();});
        }

        if (rpc.callerTimeout().count() != 0)
            timeoutScheduler_->add(rpc.callerTimeout(), requestId);

        return chit;
    }

    CallChit ongoingCall(Rpc&& rpc, OngoingCallHandler&& handler) override
    {
        using std::move;

        struct Requested
        {
            Ptr self;
            Error* errorPtr;
            OngoingCallHandler handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkReply(WampMsgType::result, ec, reply,
                                  SessionErrc::callError, handler, errorPtr))
                {
                    auto& resultMsg = message_cast<ResultMessage>(reply);
                    me.dispatchUserHandler(handler,
                                           Result({}, std::move(resultMsg)));
                }
            }
        };

        auto self = this->shared_from_this();
        auto cancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = this->ongoingRequest(
            rpc.message({}),
            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
        CallChit chit{shared_from_this(), requestId, rpc.cancelMode(), {}};

        if (cancelSlot.is_connected())
        {
            cancelSlot.assign(
                [chit](boost::asio::cancellation_type_t) {chit.cancel();});
        }

        if (rpc.callerTimeout().count() != 0)
            timeoutScheduler_->add(rpc.callerTimeout(), requestId);

        return chit;
    }

    void cancelCall(RequestId reqId, CallCancelMode mode) override
    {
        Base::cancelCall(CallCancellation{reqId, mode});
    }

    void safeCancelCall(RequestId reqId, CallCancelMode mode) override
    {
        std::weak_ptr<Client> self{shared_from_this()};
        boost::asio::dispatch(
            strand(),
            [self, reqId, mode]()
            {
                auto me = self.lock();
                if (me)
                    me->cancelCall(reqId, mode);
            });
    }

    void yield(RequestId reqId, Result&& result) override
    {
        if (!result.isProgressive())
            pendingInvocations_.erase(reqId);
        this->send(result.yieldMessage({}, reqId));
    }

    void yield(RequestId reqId, Error&& error) override
    {
        pendingInvocations_.erase(reqId);
        this->sendError(WampMsgType::invocation, reqId, std::move(error));
    }

    void safeYield(RequestId reqId, Result&& result) override
    {
        struct Dispatched
        {
            std::weak_ptr<Client> self;
            RequestId reqId;
            Result result;

            void operator()()
            {
                auto me = self.lock();
                if (me)
                    me->yield(reqId, std::move(result));
            }
        };

        boost::asio::dispatch(
            strand(),
            Dispatched{shared_from_this(), reqId, std::move(result)});
    }

    void safeYield(RequestId reqId, Error&& error) override
    {
        struct Dispatched
        {
            std::weak_ptr<Client> self;
            RequestId reqId;
            Error error;

            void operator()()
            {
                auto me = self.lock();
                if (me)
                    me->yield(reqId, std::move(error));
            }
        };

        boost::asio::dispatch(
            strand(),
            Dispatched{shared_from_this(), reqId, std::move(error)});
    }

    void initialize(
        AnyIoExecutor userExecutor,
        LogHandler warningHandler,
        LogHandler traceHandler,
        StateChangeHandler stateChangeHandler,
        ChallengeHandler challengeHandler) override
    {
        Base::setUserExecutor(std::move(userExecutor));
        warningHandler_ = std::move(warningHandler);
        Base::setTraceHandler(std::move(traceHandler));
        Base::setStateChangeHandler(std::move(stateChangeHandler));
        challengeHandler_ = std::move(challengeHandler);
    }

    void setWarningHandler(LogHandler handler) override
    {
        warningHandler_ = std::move(handler);
    }

    void setTraceHandler(LogHandler handler) override
    {
        Base::setTraceHandler(std::move(handler));
    }

    void setStateChangeHandler(StateChangeHandler handler) override
    {
        Base::setStateChangeHandler(std::move(handler));
    }

    void setChallengeHandler( ChallengeHandler handler) override
    {
        challengeHandler_ = std::move(handler);
    }

private:
    struct SubscriptionRecord
    {
        String topicUri;
        EventSlot slot;
    };

    struct RegistrationRecord
    {
        CallSlot callSlot;
        InterruptSlot interruptSlot;
    };

    using Base           = Peer<Codec, Transport>;
    using WampMsgType    = internal::WampMsgType;
    using Message        = internal::WampMessage;
    using SlotId         = uint64_t;
    using LocalSubs      = std::map<SlotId, SubscriptionRecord>;
    using Readership     = std::map<SubscriptionId, LocalSubs>;
    using TopicMap       = std::map<std::string, SubscriptionId>;
    using Registry       = std::map<RegistrationId, RegistrationRecord>;
    using InvocationMap  = std::map<RequestId, RegistrationId>;
    using CallerTimeoutDuration = typename Rpc::CallerTimeoutDuration;

    using Base::userExecutor;

    Client(TransportPtr transport)
        : Base(std::move(transport)),
          timeoutScheduler_(CallerTimeoutScheduler::create(this->strand()))
    {}

    Ptr shared_from_this()
    {
        return std::static_pointer_cast<Client>( Base::shared_from_this() );
    }

    void sendUnsubscribe(SubscriptionId subId)
    {
        struct Requested
        {
            Ptr self;

            void operator()(std::error_code ec, Message reply)
            {
                // Don't propagate WAMP errors, as we prefer
                // this to be a no-fail cleanup operation.
                self->warnReply(WampMsgType::unsubscribed, ec, reply,
                                SessionErrc::unsubscribeError);
            }
        };

        if (state() == State::established)
        {
            auto self = this->shared_from_this();
            UnsubscribeMessage msg(subId);
            this->request(msg, Requested{shared_from_this()});
        }
    }

    void sendUnsubscribe(SubscriptionId subId,
                         CompletionHandler<bool>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<bool> handler;

            void operator()(std::error_code ec, Message reply)
            {
                auto& me = *self;
                if (me.checkReply(WampMsgType::unsubscribed, ec, reply,
                                  SessionErrc::unsubscribeError, handler))
                {
                    me.dispatchUserHandler(handler, true);
                }
            }
        };

        CPPWAMP_LOGIC_CHECK((this->state() == State::established),
                            "Session is not established");
        auto self = this->shared_from_this();
        UnsubscribeMessage msg(subId);
        this->request(msg, Requested{shared_from_this(), std::move(handler)});
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

    void onWelcome(CompletionHandler<SessionInfo>&& handler, Message&& reply,
                   String&& realmUri)
    {
        WeakPtr self = this->shared_from_this();
        timeoutScheduler_->listen([self](RequestId reqId)
        {
            auto ptr = self.lock();
            if (ptr)
                ptr->cancelCall(reqId, CallCancelMode::killNoWait);
        });

        auto& welcomeMsg = message_cast<WelcomeMessage>(reply);
        SessionInfo info{{}, std::move(realmUri), std::move(welcomeMsg)};
        dispatchUserHandler(handler, std::move(info));
    }

    void onJoinAborted(CompletionHandler<SessionInfo>&& handler,
                       Message&& reply, Abort* abortPtr)
    {
        using std::move;

        auto& abortMsg = message_cast<AbortMessage>(reply);
        const auto& uri = abortMsg.reasonUri();
        SessionErrc errc;
        bool found = lookupWampErrorUri(uri, SessionErrc::joinError, errc);
        const auto& details = reply.as<Object>(1);

        if (abortPtr != nullptr)
        {
            *abortPtr = Abort({}, move(abortMsg));
        }
        else if (warningHandler_ && (!found || !details.empty()))
        {
            std::ostringstream oss;
            oss << "JOIN request aborted with error URI=" << uri;
            if (!reply.as<Object>(1).empty())
                oss << ", Details=" << reply.at(1);
            warn(oss.str());
        }

        dispatchUserHandler(handler, makeUnexpectedError(errc));
    }

    void onChallenge(Message&& msg)
    {
        auto self = this->shared_from_this();
        auto& challengeMsg = message_cast<ChallengeMessage>(msg);
        Challenge challenge({}, self, std::move(challengeMsg));

        if (challengeHandler_)
        {
            dispatchUserHandler(challengeHandler_, std::move(challenge));
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
            Event event({}, userExecutor(), std::move(eventMsg));
            for (const auto& subKv: localSubs)
                postEvent(subKv.second, event);
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

    void postEvent(const SubscriptionRecord& sub, const Event& event)
    {
        struct Posted
        {
            Ptr self;
            EventSlot slot;
            Event event;

            void operator()()
            {
                auto& me = *self;

                // Copy the subscription and publication IDs before the Event
                // object gets moved away.
                auto subId = event.subId();
                auto pubId = event.pubId();

                // The catch clauses are to prevent the publisher crashing
                // subscribers when it passes arguments having incorrect type.
                try
                {
                    slot(std::move(event));
                }
                catch (const Error& e)
                {
                    if (me.warningHandler_)
                        me.warnEventError(e, subId, pubId);
                }
                catch (const error::BadType& e)
                {
                    if (me.warningHandler_)
                        me.warnEventError(Error(e), subId, pubId);
                }
            }
        };

        auto exec = boost::asio::get_associated_executor(sub.slot,
                                                         userExecutor());
        boost::asio::post(exec, Posted{shared_from_this(), sub.slot, event});
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
            Invocation inv({}, self, userExecutor(), std::move(invMsg));
            pendingInvocations_[requestId] = regId;
            postRpcRequest(rec.callSlot, std::move(inv));
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
                using std::move;
                Interruption intr({}, self, userExecutor(), move(interruptMsg));
                postRpcRequest(rec.interruptSlot, move(intr));
            }
        }
    }

    template <typename TSlot, typename TInvocationOrInterruption>
    void postRpcRequest(TSlot slot, TInvocationOrInterruption&& request)
    {
        using std::move;

        struct Posted
        {
            Ptr self;
            TSlot slot;
            TInvocationOrInterruption request;

            void operator()()
            {
                auto& me = *self;

                // Copy the request ID before the request object gets moved away.
                auto requestId = request.requestId();

                try
                {
                    Outcome outcome(slot(move(request)));
                    switch (outcome.type())
                    {
                    case Outcome::Type::deferred:
                        // Do nothing
                        break;

                    case Outcome::Type::result:
                        me.safeYield(requestId, move(outcome).asResult());
                        break;

                    case Outcome::Type::error:
                        me.safeYield(requestId, move(outcome).asError());
                        break;

                    default:
                        assert(false && "unexpected Outcome::Type");
                    }
                }
                catch (Error& error)
                {
                    me.yield(requestId, move(error));
                }
                catch (const error::BadType& e)
                {
                    // Forward Variant conversion exceptions as ERROR messages.
                    me.yield(requestId, Error(e));
                }
            }
        };

        auto exec = boost::asio::get_associated_executor(slot, userExecutor());
        boost::asio::post(
            exec,
            Posted{shared_from_this(), move(slot), move(request)});
    }

    template <typename THandler>
    bool checkError(std::error_code ec, THandler& handler)
    {
        if (ec)
            dispatchUserHandler(handler, UnexpectedError(ec));
        return !ec;
    }

    template <typename THandler>
    bool checkReply(WampMsgType type, std::error_code ec, Message& reply,
                    SessionErrc defaultErrc, THandler& handler,
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
                SessionErrc errc;
                bool found = lookupWampErrorUri(uri, defaultErrc, errc);
                bool hasArgs = !errMsg.args().empty() ||
                               !errMsg.kwargs().empty();
                if (errorPtr != nullptr)
                {
                    *errorPtr = Error({}, std::move(errMsg));
                }
                else if (warningHandler_ && (!found || hasArgs))
                {
                    std::ostringstream oss;
                    oss << "Expected " << MessageTraits::lookup(type).name
                        << " reply but got ERROR with URI=" << uri;
                    if (!errMsg.args().empty())
                        oss << ", Args=" << errMsg.args();
                    if (!errMsg.kwargs().empty())
                        oss << ", ArgsKv=" << errMsg.kwargs();
                    warn(oss.str());
                }

                dispatchUserHandler(handler, makeUnexpectedError(errc));
            }
            else
                assert((reply.type() == type) && "Unexpected WAMP message type");
        }
        return success;
    }

    void warnReply(WampMsgType type, std::error_code ec, Message& reply,
                   SessionErrc defaultErrc)
    {
        if (ec)
        {
            warn(error::Failure::makeMessage(ec));
        }
        else if (reply.type() == WampMsgType::error)
        {
            auto& errMsg = message_cast<ErrorMessage>(reply);
            const auto& uri = errMsg.reasonUri();
            std::ostringstream oss;
            oss << "Expected " << MessageTraits::lookup(type).name
                << " reply but got ERROR with URI=" << uri;
            if (!errMsg.args().empty())
                oss << ", Args=" << errMsg.args();
            if (!errMsg.kwargs().empty())
                oss << ", ArgsKv=" << errMsg.kwargs();
            warn(oss.str());
        }
        else
        {
            assert((reply.type() == type) && "Unexpected WAMP message type");
        }
    }

    void warn(std::string log)
    {
        if (warningHandler_)
            dispatchUserHandler(warningHandler_, std::move(log));
    }

    template <typename S, typename... Ts>
    void dispatchUserHandler(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        dispatchVia(userExecutor(), std::move(handler),
                    std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void dispatchUserHandler(const AnyReusableHandler<S>& handler, Ts&&... args)
    {
        dispatchVia(userExecutor(), handler, std::forward<Ts>(args)...);
    }

    SlotId nextSlotId() {return nextSlotId_++;}

    SlotId nextSlotId_ = 0;
    TopicMap topics_;
    Readership readership_;
    Registry registry_;
    InvocationMap pendingInvocations_;
    CallerTimeoutScheduler::Ptr timeoutScheduler_;
    LogHandler warningHandler_;
    ChallengeHandler challengeHandler_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
