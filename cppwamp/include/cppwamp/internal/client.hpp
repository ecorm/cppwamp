/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENT_HPP
#define CPPWAMP_INTERNAL_CLIENT_HPP

#include <atomic>
#include <cassert>
#include <exception>
#include <future>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <boost/asio/post.hpp>
#include "../anyhandler.hpp"
#include "../calleestreaming.hpp"
#include "../callerstreaming.hpp"
#include "../codec.hpp"
#include "../cancellation.hpp"
#include "../connector.hpp"
#include "../logging.hpp"
#include "../peerdata.hpp"
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../transport.hpp"
#include "../version.hpp"
#include "callee.hpp"
#include "caller.hpp"
#include "challengee.hpp"
#include "matchuri.hpp"
#include "streamchannel.hpp"
#include "subscriber.hpp"
#include "peer.hpp"
#include "timeoutscheduler.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct StreamRegistration
{
    AnyReusableHandler<void (CalleeChannel)> streamSlot;
    bool invitationExpected;
};

//------------------------------------------------------------------------------
class StreamRecord
{
public:
    using CompletionHandler =
        AnyCompletionHandler<void (ErrorOr<CallerChannel>)>;

    explicit StreamRecord(CallerChannelImpl::Ptr c, StreamRequest& i,
                          CompletionHandler&& f = {})
        : StreamRecord(std::move(c), i.error({}), std::move(f))
    {}

    void onReply(WampMessage&& msg, AnyIoExecutor& exec,
                 AnyCompletionExecutor& userExec)
    {
        if (msg.type() == WampMsgType::result)
            onResult(messageCast<ResultMessage>(msg), exec, userExec);
        else
            onError(messageCast<ErrorMessage>(msg), exec, userExec);
    }

    void cancel(AnyIoExecutor& exec, AnyCompletionExecutor& userExec)
    {
        abandon(makeUnexpectedError(WampErrc::cancelled), exec, userExec);
    }

    void abandon(UnexpectedError unex, AnyIoExecutor& exec,
                 AnyCompletionExecutor& userExec)
    {
        if (handler_)
            postVia(exec, userExec, std::move(handler_), unex);
        handler_ = nullptr;
        channel_.reset();
        weakChannel_.reset();
    }

private:
    explicit StreamRecord(CallerChannelImpl::Ptr c, Error* e,
                          CompletionHandler&& f)
        : handler_(std::move(f)),
          channel_(std::move(c)),
          weakChannel_(channel_),
          errorPtr_(e)
    {}

    void onResult(ResultMessage& msg, AnyIoExecutor& exec,
                  AnyCompletionExecutor& userExec)
    {
        if (channel_)
        {
            if (channel_->expectsRsvp())
                channel_->setRsvp(std::move(msg));

            if (handler_)
            {
                dispatchVia(exec, userExec, std::move(handler_),
                            CallerChannel{{}, channel_});
                handler_ = nullptr;
            }

            if (!channel_->expectsRsvp())
                channel_->postResult(std::move(msg));

            channel_.reset();
        }
        else
        {
            auto channel = weakChannel_.lock();
            if (channel)
                channel->postResult(std::move(msg));
        }
    }

    void onError(ErrorMessage& msg, AnyIoExecutor& exec,
                 AnyCompletionExecutor& userExec)
    {
        if (channel_)
        {
            if (handler_)
            {
                Error error{{}, std::move(msg)};
                auto unex = makeUnexpectedError(error.errorCode());
                if (errorPtr_)
                    *errorPtr_ = std::move(error);
                dispatchVia(exec, userExec, std::move(handler_), unex);
                handler_ = nullptr;
            }
            else
            {
                channel_->postError(std::move(msg));
            }
            channel_.reset();
        }
        else
        {
            auto channel = weakChannel_.lock();
            if (channel)
                channel->postError(std::move(msg));
        }
    }

    CompletionHandler handler_;
    CallerChannelImpl::Ptr channel_;
    CallerChannelImpl::WeakPtr weakChannel_;
    Error* errorPtr_ = nullptr;
};

//------------------------------------------------------------------------------
class Requestor
{
public:
    using Message = WampMessage;
    using RequestHandler = AnyCompletionHandler<void (ErrorOr<Message>)>;
    using StreamRequestHandler =
        AnyCompletionHandler<void (ErrorOr<CallerChannel>)>;
    using ChunkSlot = AnyReusableHandler<void (CallerChannel,
                                               ErrorOr<CallerInputChunk>)>;

    Requestor(Peer& peer, IoStrand strand, AnyIoExecutor exec,
              AnyCompletionExecutor userExec)
        : strand_(std::move(strand)),
          executor_(std::move(exec)),
          userExecutor_(std::move(userExec)),
          peer_(peer)
    {}

    ErrorOr<RequestId> request(Message& msg, RequestHandler&& handler)
    {
        assert(msg.type() != WampMsgType::none);
        RequestId requestId = nullId();
        if (msg.isRequest())
        {
            // Will take 285 years to overflow 2^53 at 1 million requests/sec
            assert(nextRequestId_ < 9007199254740992u);
            requestId = nextRequestId_ + 1;
            msg.setRequestId(requestId);
        }

        auto sent = peer_.send(msg);
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        if (msg.isRequest())
            ++nextRequestId_;

        auto emplaced = requests_.emplace(msg.requestKey(), std::move(handler));
        assert(emplaced.second);
        return requestId;
    }

    ErrorOr<CallerChannel> requestStream(
        bool rsvpExpected, Caller::WeakPtr caller, StreamRequest&& req,
        ChunkSlot&& onChunk, StreamRequestHandler&& handler = {})
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        assert(nextRequestId_ < 9007199254740992u);
        ChannelId channelId = nextRequestId_ + 1;
        auto uri = req.uri();
        auto& msg = req.callMessage({}, channelId);

        auto sent = peer_.send(msg);
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        ++nextRequestId_;

        auto channel = std::make_shared<CallerChannelImpl>(
            channelId, std::move(uri), req.mode(), req.cancelMode(),
            rsvpExpected, std::move(caller), std::move(onChunk), executor_,
            userExecutor_);
        auto emplaced = channels_.emplace(
            channelId, StreamRecord{channel, req, std::move(handler)});
        assert(emplaced.second);
        return CallerChannel{{}, std::move(channel)};
    }

    bool onReply(Message&& msg) // Returns true if request was found
    {
        assert(msg.isReply());

        {
            auto kv = requests_.find(msg.requestKey());
            if (kv != requests_.end())
            {
                auto handler = std::move(kv->second);
                requests_.erase(kv);
                handler(std::move(msg));
                return true;
            }
        }

        {
            if (msg.repliesTo() != WampMsgType::call)
                return false;
            auto kv = channels_.find(msg.requestId());
            if (kv == channels_.end())
                return false;

            if (msg.isProgressive())
            {
                StreamRecord& req = kv->second;
                req.onReply(std::move(msg), executor_, userExecutor_);
            }
            else
            {
                StreamRecord req{std::move(kv->second)};
                channels_.erase(kv);
                req.onReply(std::move(msg), executor_, userExecutor_);
            }
        }

        return true;
    }

    // Returns true if request was found
    bool cancelCall(const CallCancellation& cancellation)
    {
        // If the cancel mode is not 'kill', don't wait for the router's
        // ERROR message and post the request handler immediately
        // with a WampErrc::cancelled error code.

        auto unex = makeUnexpectedError(WampErrc::cancelled);

        {
            RequestKey key{WampMsgType::call, cancellation.requestId()};
            auto kv = requests_.find(key);
            if (kv != requests_.end())
            {
                if (cancellation.mode() != CallCancelMode::kill)
                {
                    auto handler = std::move(kv->second);
                    requests_.erase(kv);
                    completeRequest(handler, unex);
                }
                return true;
            }
        }

        {
            auto kv = channels_.find(cancellation.requestId());
            if (kv == channels_.end())
                return false;

            if (cancellation.mode() != CallCancelMode::kill)
            {
                StreamRecord req{std::move(kv->second)};
                channels_.erase(kv);
                req.cancel(executor_, userExecutor_);
            }
        }

        return true;
    }

    ErrorOrDone sendCallerChunk(RequestId reqId, CallerOutputChunk&& chunk)
    {
        RequestKey key{WampMsgType::call, reqId};
        if (requests_.count(key) == 0 && channels_.count(reqId) == 0)
            return false;
        return peer_.send(chunk.callMessage({}, reqId));
    }

    void abandonAll(std::error_code ec)
    {
        UnexpectedError unex{ec};
        for (auto& kv: requests_)
            completeRequest(kv.second, unex);
        for (auto& kv: channels_)
            kv.second.abandon(unex, executor_, userExecutor_);
        clear();
    }

    void clear()
    {
        requests_.clear();
        channels_.clear();
        nextRequestId_ = nullId();
    }

private:
    using RequestKey = typename Message::RequestKey;

    template <typename F, typename... Ts>
    void completeRequest(F& handler, Ts&&... args)
    {
        boost::asio::post(
            strand_,
            std::bind(std::move(handler), std::forward<Ts>(args)...));
    }

    IoStrand strand_;
    std::map<RequestKey, RequestHandler> requests_;
    std::map<ChannelId, StreamRecord> channels_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    Peer& peer_;
    RequestId nextRequestId_ = nullId();
};


//------------------------------------------------------------------------------
class Readership
{
public:
    using SubscriberPtr = std::shared_ptr<Subscriber>;
    using EventSlot = AnyReusableHandler<void (Event)>;

    Readership(AnyIoExecutor exec, AnyCompletionExecutor userExec)
        : executor_(std::move(exec)),
          userExecutor_(std::move(userExec))
    {}

    Subscription subscribe(MatchUri topic, EventSlot& slot,
                           SubscriberPtr subscriber)
    {
        assert(topic.policy() != MatchPolicy::unknown);
        auto kv = byTopic_.find(topic);
        if (kv == byTopic_.end())
            return {};

        return addSlotToExisingSubscription(
            kv->second, std::move(topic), std::move(slot),
            std::move(subscriber));
    }

    Subscription onSubscribed(const SubscribedMessage& msg, MatchUri topic,
                              EventSlot&& slot, SubscriberPtr subscriber)
    {
        // Check if the router treats the topic as belonging to an existing
        // subcription.
        auto subId = msg.subscriptionId();
        auto kv = subscriptions_.find(subId);
        if (kv != subscriptions_.end())
        {
            return addSlotToExisingSubscription(
                       kv, std::move(topic), std::move(slot),
                       std::move(subscriber));
        }

        auto slotId = nextSlotId();
        auto emplaced =
            subscriptions_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(subId),
                std::forward_as_tuple(topic, slotId, std::move(slot)));
        assert(emplaced.second);

        auto emplaced2 = byTopic_.emplace(std::move(topic), emplaced.first);
        assert(emplaced2.second);

        return Subscription({}, subscriber, subId, slotId);
    }

    // Returns true if the last local slot was removed from a subscription
    // and the client needs to send an UNSUBSCRIBE message.
    bool unsubscribe(const Subscription& sub)
    {
        auto kv = subscriptions_.find(sub.id());
        if (kv == subscriptions_.end())
            return false;

        auto& record = kv->second;
        record.slots.erase(sub.slotId({}));
        if (!record.slots.empty())
            return false;

        byTopic_.erase(record.topic);
        subscriptions_.erase(kv);
        return true;
    }

    // Returns true if there are any subscriptions matching the event
    bool onEvent(EventMessage& eventMsg, SubscriberPtr subscriber)
    {
        auto found = subscriptions_.find(eventMsg.subscriptionId());
        if (found == subscriptions_.end())
            return false;

        const auto& record = found->second;
        assert(!record.slots.empty());
        Event event({}, userExecutor_, std::move(eventMsg));
        for (const auto& kv: record.slots)
        {
            const auto& slot = kv.second;
            postEvent(slot, record.topic.uri(), event, subscriber, executor_,
                      userExecutor_);
        }
        return true;
    }

    void clear()
    {
        byTopic_.clear();
        subscriptions_.clear();
        nextSlotId_ = nullId();
    }

private:
    using SlotId = uint64_t;

    struct Record
    {
        Record(MatchUri topic, SlotId slotId, EventSlot&& slot)
            : topic(std::move(topic))
        {
            slots.emplace(slotId, std::move(slot));
        }

        std::map<SlotId, EventSlot> slots;
        MatchUri topic;
    };

    using SubscriptionMap = std::map<SubscriptionId, Record>;
    using ByTopic = std::map<MatchUri, SubscriptionMap::iterator>;

    Subscription addSlotToExisingSubscription(
        SubscriptionMap::iterator iter, MatchUri&& topic, EventSlot&& slot,
        SubscriberPtr&& subscriber)
    {
        auto subId = iter->first;
        auto& record = iter->second;
        auto slotId = nextSlotId();
        auto emplaced = record.slots.emplace(slotId, std::move(slot));
        assert(emplaced.second);
        return Subscription{{}, subscriber, subId, slotId};
    }

    void postEvent(const EventSlot& slot, const Uri& uri, const Event& event,
                   SubscriberPtr subscriber, AnyIoExecutor& exec,
                   AnyCompletionExecutor& userExec)
    {
        struct Posted
        {
            SubscriberPtr subscriber;
            Event event;
            Uri uri;
            EventSlot slot;

            void operator()()
            {
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
                    subscriber->onEventError(e, uri, subId, pubId);
                }
                catch (const error::BadType& e)
                {
                    subscriber->onEventError(Error(e), uri, subId, pubId);
                }
            }
        };

        auto associatedExec =
            boost::asio::get_associated_executor(slot, userExec);
        Posted posted{subscriber, event, uri, slot};
        boost::asio::post(
            exec,
            boost::asio::bind_executor(associatedExec, std::move(posted)));
    }

    SlotId nextSlotId() {return nextSlotId_++;}

    SubscriptionMap subscriptions_;
    ByTopic byTopic_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    SlotId nextSlotId_ = 0;
};


//------------------------------------------------------------------------------
struct ProcedureRegistration
{
    using CallSlot = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;

    // TODO: Bind the userExecutor_ (if necessary) upon registration instead
    // of upon invocation.
    CallSlot callSlot;
    InterruptSlot interruptSlot;
};


//------------------------------------------------------------------------------
struct InvocationRecord
{
    InvocationRecord(RegistrationId regId) : registrationId(regId) {}

    CalleeChannelImpl::WeakPtr channel;
    RegistrationId registrationId;
    bool invoked = false;     // Set upon the first streaming invocation
    bool interrupted = false; // Set when an interruption was received
                              //     for this invocation.
    bool moot = false;        // Set when auto-responding to an interruption
                              //     with an error.
    bool closed = false;      // Set when the initiating or subsequent
                              //     invocation is not progressive
};

//------------------------------------------------------------------------------
class ProcedureRegistry
{
public:
    using CalleePtr     = std::shared_ptr<Callee>;
    using CallSlot      = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;
    using StreamSlot    = AnyReusableHandler<void (CalleeChannel)>;

    ProcedureRegistry(Peer& peer, AnyIoExecutor exec,
                      AnyCompletionExecutor userExec)
        : executor_(std::move(exec)),
          userExecutor_(std::move(userExec)),
          peer_(peer)
    {}

    ErrorOr<Registration> enroll(ProcedureRegistration&& reg, CalleePtr callee,
                                 const RegisteredMessage& msg)
    {
        auto regId = msg.registrationId();
        auto emplaced = procedures_.emplace(regId, std::move(reg));
        if (!emplaced.second)
            return makeUnexpectedError(WampErrc::procedureAlreadyExists);
        return Registration{{}, callee, regId};
    }

    ErrorOr<Registration> enroll(StreamRegistration&& reg, CalleePtr callee,
                                 const RegisteredMessage& msg)
    {
        auto regId = msg.registrationId();
        auto emplaced = streams_.emplace(regId, std::move(reg));
        if (!emplaced.second)
            return makeUnexpectedError(WampErrc::procedureAlreadyExists);
        return Registration{{}, callee, regId};
    }

    bool unregister(const Registration& reg)
    {
        bool erased = procedures_.erase(reg.id()) != 0;
        if (!erased)
            erased = streams_.erase(reg.id()) != 0;
        return erased;
    }

    ErrorOrDone yield(RequestId reqId, Result&& result)
    {
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        bool erased = !result.isProgressive() || moot;
        if (erased)
            invocations_.erase(found);
        if (moot)
            return false;

        auto done = peer_.send(result.yieldMessage({}, reqId));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            if (!erased)
                invocations_.erase(found);
            peer_.sendError(WampMsgType::invocation, reqId,
                            Error{WampErrc::payloadSizeExceeded});
        }
        return done;
    }

    ErrorOrDone yield(RequestId reqId, CalleeOutputChunk&& chunk)
    {
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        bool erased = chunk.isFinal() || moot;
        if (erased)
            invocations_.erase(found);
        if (moot)
            return false;

        auto done = peer_.send(chunk.yieldMessage({}, reqId));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            if (!erased)
                invocations_.erase(found);
            peer_.sendError(WampMsgType::invocation, reqId,
                            Error{WampErrc::payloadSizeExceeded});
        }
        return done;
    }

    ErrorOrDone yield(RequestId reqId, Error&& error)
    {
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        invocations_.erase(found);
        if (moot)
            return false;

        return peer_.sendError(WampMsgType::invocation, reqId,
                               std::move(error));
    }

    WampErrc onInvocation(InvocationMessage& msg, CalleePtr callee)
    {
        auto requestId = msg.requestId();
        auto regId = msg.registrationId();

        {
            auto kv = procedures_.find(regId);
            if (kv != procedures_.end())
            {
                return onProcedureInvocation(msg, std::move(callee),
                                             regId, kv->second);
            }
        }

        {
            auto kv = streams_.find(regId);
            if (kv != streams_.end())
            {
                return onStreamInvocation(msg, std::move(callee),
                                          regId, kv->second);
            }
        }

        peer_.sendError(WampMsgType::invocation, requestId,
                        {WampErrc::noSuchProcedure});
        return WampErrc::noSuchProcedure;
    }

    void onInterrupt(InterruptMessage& msg, CalleePtr callee)
    {
        auto found = invocations_.find(msg.requestId());
        if (found == invocations_.end())
            return;

        InvocationRecord& rec = found->second;
        if (rec.interrupted)
            return;
        rec.interrupted = true;

        bool interruptHandled = false;

        {
            auto kv = procedures_.find(rec.registrationId);
            if (kv != procedures_.end())
            {
                interruptHandled =
                    onProcedureInterruption(msg, std::move(callee), kv->second);
            }
        }

        {
            auto kv = streams_.find(rec.registrationId);
            if (kv != streams_.end())
                interruptHandled = postStreamInterruption(msg, rec);
        }

        if (!interruptHandled)
            automaticallyRespondToInterruption(msg, rec);
    }

    void clear()
    {
        procedures_.clear();
        invocations_.clear();
    }

private:
    using InvocationMap = std::map<RequestId, InvocationRecord>;
    using ProcedureMap = std::map<RegistrationId, ProcedureRegistration>;
    using StreamMap = std::map<RegistrationId, StreamRegistration>;

    WampErrc onProcedureInvocation(InvocationMessage& msg, CalleePtr callee,
                                   RegistrationId regId,
                                   const ProcedureRegistration& reg)
    {
        auto requestId = msg.requestId();
        auto emplaced = invocations_.emplace(requestId,
                                             InvocationRecord{regId});

        // Progressive calls not allowed on procedures not registered
        // as streams.
        if (!emplaced.second)
            return WampErrc::optionNotAllowed;

        auto& invocationRec = emplaced.first->second;
        invocationRec.closed = true;
        Invocation inv({}, callee, userExecutor_, std::move(msg));
        postRpcRequest(reg.callSlot, std::move(inv), callee);
        return WampErrc::success;
    }

    bool onProcedureInterruption(InterruptMessage& msg, CalleePtr callee,
                                 const ProcedureRegistration& reg)
    {
        if (reg.interruptSlot == nullptr)
            return false;
        Interruption intr({}, callee, userExecutor_, std::move(msg));
        postRpcRequest(reg.interruptSlot, std::move(intr), callee);
        return true;
    }

    template <typename TSlot, typename TInvocationOrInterruption>
    void postRpcRequest(TSlot slot, TInvocationOrInterruption&& request,
                        CalleePtr callee)
    {
        struct Posted
        {
            CalleePtr callee;
            TSlot slot;
            TInvocationOrInterruption request;

            void operator()()
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
                        callee->safeYield(requestId,
                                          std::move(outcome).asResult());
                        break;

                    case Outcome::Type::error:
                        callee->safeYield(requestId,
                                          std::move(outcome).asError());
                        break;

                    default:
                        assert(false && "unexpected Outcome::Type");
                    }
                }
                catch (Error& error)
                {
                    callee->yield(requestId, std::move(error));
                }
                catch (const error::BadType& e)
                {
                    // Forward Variant conversion exceptions as ERROR messages.
                    callee->yield(requestId, Error(e)).value();
                }
            }
        };

        auto associatedExec =
            boost::asio::get_associated_executor(slot, userExecutor_);
        Posted posted{callee, std::move(slot), std::move(request)};
        boost::asio::post(
            executor_,
            boost::asio::bind_executor(associatedExec, std::move(posted)));
    }

    WampErrc onStreamInvocation(InvocationMessage& msg, CalleePtr callee,
                                RegistrationId regId,
                                const StreamRegistration& reg)
    {
        auto requestId = msg.requestId();
        auto emplaced = invocations_.emplace(requestId,
                                             InvocationRecord{regId});
        auto& invocationRec = emplaced.first->second;
        if (invocationRec.closed)
            return WampErrc::protocolViolation;

        invocationRec.closed = !msg.isProgressive();
        processStreamInvocation(reg, invocationRec, std::move(msg), callee);
        return WampErrc::success;
    }

    void processStreamInvocation(const StreamRegistration& reg,
                              InvocationRecord& rec,
                              InvocationMessage&& msg,
                              CalleePtr callee)
    {
        if (!rec.invoked)
        {
            auto channel = std::make_shared<CalleeChannelImpl>(
                std::move(msg), reg.invitationExpected, executor_,
                userExecutor_, std::move(callee));
            rec.channel = channel;
            rec.invoked = true;

            // Execute the slot directly from this strand in order to avoid
            // race condition between accept and postInvocation/postInterrupt
            // on the CalleeChannel.
            CalleeChannel proxy{{}, std::move(channel)};
            reg.streamSlot(std::move(proxy));
        }
        else
        {
            auto channel = rec.channel.lock();
            if (channel)
            {
                channel->postInvocation(std::move(msg));
            }
        }
    }

    bool postStreamInterruption(InterruptMessage& msg, InvocationRecord& rec)
    {
        auto channel = rec.channel.lock();
        return bool(channel) &&
               channel->postInterrupt(std::move(msg));
    }

    void automaticallyRespondToInterruption(InterruptMessage& msg,
                                            InvocationRecord& rec)
    {
        // Respond immediately when cancel mode is 'kill' and no interrupt
        // slot is provided.
        // Dealer will have already responded in `killnowait` mode.
        // Dealer does not emit an INTERRUPT in `skip` mode.
        Interruption intr({}, std::move(msg));
        if (intr.cancelMode() == CallCancelMode::kill)
        {
            rec.moot = true;
            Error error{intr.reason().value_or(
                errorCodeToUri(WampErrc::cancelled))};
            peer_.sendError(WampMsgType::invocation, intr.requestId(),
                            std::move(error));
        }
    }

    ProcedureMap procedures_;
    StreamMap streams_;
    InvocationMap invocations_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    Peer& peer_;
};


//------------------------------------------------------------------------------
// Provides the WAMP client implementation.
//------------------------------------------------------------------------------
class Client : public std::enable_shared_from_this<Client>, public Callee,
               public Caller, public Subscriber, public Challengee
{
public:
    using Ptr                = std::shared_ptr<Client>;
    using TransportPtr       = Transporting::Ptr;
    using State              = SessionState;
    using FutureErrorOrDone  = std::future<ErrorOrDone>;
    using EventSlot          = AnyReusableHandler<void (Event)>;
    using CallSlot           = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot      = AnyReusableHandler<Outcome (Interruption)>;
    using StreamSlot         = AnyReusableHandler<void (CalleeChannel)>;
    using LogHandler         = AnyReusableHandler<void (LogEntry)>;
    using StateChangeHandler = AnyReusableHandler<void (SessionState,
                                                        std::error_code)>;
    using ChallengeHandler   = AnyReusableHandler<void (Challenge)>;
    using OngoingCallHandler = AnyReusableHandler<void (ErrorOr<Result>)>;
    using ChunkSlot = AnyReusableHandler<void (CallerChannel,
                                               ErrorOr<CallerInputChunk>)>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(AnyIoExecutor exec)
    {
        return Ptr(new Client(exec, exec));
    }

    static Ptr create(const AnyIoExecutor& exec, AnyCompletionExecutor userExec)
    {
        return Ptr(new Client(exec, std::move(userExec)));
    }

    static const Object& roles()
    {
        static const Object rolesDict =
        {
            {"callee", Object{{"features", Object{{
                {"call_canceling", true},
                {"call_timeout", true},
                {"call_trustlevels", true},
                {"caller_identification", true},
                {"pattern_based_registration", true},
                {"progressive_call_results", true},
                {"progressive_calls", true}
            }}}}},
            {"caller", Object{{"features", Object{{
                {"call_canceling", true},
                {"call_timeout", true},
                {"caller_identification", true},
                {"progressive_call_results", true},
                {"progressive_calls", true}
            }}}}},
            {"publisher", Object{{"features", Object{{
                {"publisher_exclusion", true},
                {"publisher_identification", true},
                {"subscriber_blackwhite_listing", true}
            }}}}},
            {"subscriber", Object{{"features", Object{{
                {"pattern_based_subscription", true},
                {"publication_trustlevels", true},
                {"publisher_identification", true},
            }}}}}
        };
        return rolesDict;
    }

    State state() const {return peer_.state();}

    const AnyIoExecutor& executor() const {return executor_;}

    const AnyCompletionExecutor& userExecutor() const {return userExecutor_;}

    const IoStrand& strand() const {return strand_;}

    void setLogHandler(LogHandler handler) {logHandler_ = std::move(handler);}

    void setLogLevel(LogLevel level) {peer_.setLogLevel(level);}

    void safeSetLogHandler(LogHandler f)
    {
        struct Dispatched
        {
            Ptr self;
            LogHandler f;
            void operator()() {self->setLogHandler(std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(f));
    }

    void setStateChangeHandler(StateChangeHandler f)
    {
        stateChangeHandler_ = std::move(f);
    }

    void safeSetStateChangeHandler(StateChangeHandler f)
    {
        struct Dispatched
        {
            Ptr self;
            StateChangeHandler f;
            void operator()() {self->setStateChangeHandler(std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(f));
    }

    void connect(ConnectionWishList&& wishes,
                 CompletionHandler<size_t>&& handler)
    {
        assert(!wishes.empty());

        if (!checkState(State::disconnected, handler))
            return;

        isTerminating_ = false;
        peer_.startConnecting();
        currentConnector_ = nullptr;

        // This makes it easier to transport the move-only completion handler
        // through the gauntlet of intermediary handler functions.
        auto sharedHandler =
            std::make_shared<CompletionHandler<size_t>>(std::move(handler));

        doConnect(std::move(wishes), 0, std::move(sharedHandler));
    }

    void safeConnect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            ConnectionWishList w;
            CompletionHandler<size_t> f;
            void operator()() {self->connect(std::move(w), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(w), std::move(f));
    }

    void join(Realm&& realm, ChallengeHandler onChallenge,
              CompletionHandler<Welcome>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<Welcome> handler;
            String realmUri;
            Reason* abortPtr;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                me.challengeHandler_ = nullptr;
                if (me.checkError(reply, handler))
                {
                    if (reply->type() == WampMsgType::welcome)
                    {
                        me.onWelcome(std::move(handler), *reply,
                                     std::move(realmUri));
                    }
                    else
                    {
                        assert(reply->type() == WampMsgType::abort);
                        me.onJoinAborted(std::move(handler), *reply, abortPtr);
                    }
                }
            }
        };

        if (!checkState(State::closed, handler))
            return;

        realm.withOption("agent", Version::agentString())
             .withOption("roles", roles());
        challengeHandler_ = std::move(onChallenge);
        peer_.establishSession();
        request(realm.message({}),
                Requested{shared_from_this(), std::move(handler),
                          realm.uri(), realm.abortReason({})});
    }

    void safeJoin(Realm&& r, ChallengeHandler c, CompletionHandler<Welcome>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Realm r;
            ChallengeHandler c;
            CompletionHandler<Welcome> f;
            void operator()() {self->join(std::move(r), std::move(c),
                               std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(r), std::move(c), std::move(f));
    }

    ErrorOrDone authenticate(Authentication&& auth) override
    {
        if (state() != State::authenticating)
            return makeUnexpectedError(Errc::invalidState);
        return peer_.send(auth.message({}));
    }

    FutureErrorOrDone safeAuthenticate(Authentication&& a) override
    {
        struct Dispatched
        {
            Ptr self;
            Authentication a;
            ErrorOrDonePromise p;

            void operator()()
            {
                try
                {
                    p.set_value(self->authenticate(std::move(a)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(std::move(a), std::move(p));
        return fut;
    }

    void leave(Reason&& reason, CompletionHandler<Reason>&& handler)
    {
        struct Adjourned
        {
            Ptr self;
            CompletionHandler<Reason> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                me.clear();
                if (me.checkError(reply, handler))
                {
                    me.peer_.close();
                    me.abortPending(Errc::abandoned);
                    auto& goodBye = messageCast<GoodbyeMessage>(*reply);
                    me.completeNow(handler, Reason({}, std::move(goodBye)));
                }
            }
        };

        if (!checkState(State::established, handler))
            return;
        if (reason.uri().empty())
            reason.setUri({}, errorCodeToUri(WampErrc::closeRealm));

        timeoutScheduler_->clear();
        peer_.startShuttingDown();
        request(reason.message({}),
                Adjourned{shared_from_this(), std::move(handler)});
    }

    void safeLeave(Reason&& r, CompletionHandler<Reason>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            CompletionHandler<Reason> f;
            void operator()() {self->leave(std::move(r), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(r), std::move(f));
    }

    void disconnect()
    {
        if (state() == State::connecting)
            currentConnector_->cancel();
        abortPending(Errc::abandoned);
        clear();
        peer_.disconnect();
    }

    void safeDisconnect()
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->disconnect();}
        };

        safelyDispatch<Dispatched>();
    }

    void terminate()
    {
        isTerminating_ = true;
        if (state() == State::connecting)
            currentConnector_->cancel();
        clear();
        peer_.disconnect();
    }

    void safeTerminate()
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->terminate();}
        };

        safelyDispatch<Dispatched>();
    }

    void subscribe(Topic&& topic, EventSlot&& slot,
                   CompletionHandler<Subscription>&& handler)
    {
        struct Requested
        {
            Ptr self;
            MatchUri matchUri;
            EventSlot slot;
            CompletionHandler<Subscription> handler;
            MatchPolicy policy;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (!me.checkReply(reply, WampMsgType::subscribed, handler))
                    return;
                const auto& msg = messageCast<SubscribedMessage>(*reply);
                auto sub = me.readership_.onSubscribed(
                    msg, std::move(matchUri), std::move(slot), self);
                me.completeNow(handler, std::move(sub));
            }
        };

        if (!checkState(State::established, handler))
            return;

        auto self = shared_from_this();
        MatchUri matchUri{topic};
        auto subscription = readership_.subscribe(matchUri, slot, self);
        if (subscription)
            return complete(handler, std::move(subscription));

        auto topicUri = topic.uri();
        auto policy = topic.matchPolicy();
        request(topic.message({}),
                Requested{shared_from_this(), std::move(matchUri),
                          std::move(slot), std::move(handler), policy});
    }

    void safeSubscribe(Topic&& t, EventSlot&& s,
                       CompletionHandler<Subscription>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Topic t;
            EventSlot s;
            CompletionHandler<Subscription> f;

            void operator()()
            {
                self->subscribe(std::move(t), std::move(s), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(t), std::move(s), std::move(f));
    }

    void unsubscribe(const Subscription& sub) override
    {
        if (readership_.unsubscribe(sub))
            sendUnsubscribe(sub.id());
    }

    void safeUnsubscribe(const Subscription& s) override
    {
        struct Dispatched
        {
            Ptr self;
            Subscription s;
            void operator()() {self->unsubscribe(s);}
        };

        safelyDispatch<Dispatched>(s);
    }

    void unsubscribe(const Subscription& sub, CompletionHandler<bool>&& handler)
    {
        if (readership_.unsubscribe(sub))
            sendUnsubscribe(sub.id(), std::move(handler));
        else
            complete(handler, false);
    }

    void safeUnsubscribe(const Subscription& s, CompletionHandler<bool>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Subscription s;
            CompletionHandler<bool> f;
            void operator()() {self->unsubscribe(s, std::move(f));}
        };

        safelyDispatch<Dispatched>(s, std::move(f));
    }

    void onEventError(const Error& error, const std::string& topicUri,
                      SubscriptionId subId, PublicationId pubId) override
    {
        if (logLevel() <= LogLevel::error)
        {
            std::ostringstream oss;
            oss << "EVENT handler reported an ";
            outputErrorDetails(oss, error);
            oss << ", for topic=" << topicUri
                << ", subscriptionId=" << subId
                << ", publicationId=" << pubId << ")";
            log(LogLevel::error, oss.str());
        }
    }

    ErrorOrDone publish(Pub&& pub)
    {
        if (state() != State::established)
            return makeUnexpectedError(Errc::invalidState);
        return peer_.send(pub.message({}));
    }

    FutureErrorOrDone safePublish(Pub&& p)
    {
        struct Dispatched
        {
            Ptr self;
            Pub p;
            ErrorOrDonePromise prom;

            void operator()()
            {
                try
                {
                    prom.set_value(self->publish(std::move(p)));
                }
                catch (...)
                {
                    prom.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise prom;
        auto fut = prom.get_future();
        safelyDispatch<Dispatched>(std::move(p), std::move(prom));
        return fut;
    }

    void publish(Pub&& pub, CompletionHandler<PublicationId>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<PublicationId> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, WampMsgType::published, handler))
                {
                    const auto& pubMsg = messageCast<PublishedMessage>(*reply);
                    me.completeNow(handler, pubMsg.publicationId());
                }
            }
        };

        if (!checkState(State::established, handler))
            return;

        pub.withOption("acknowledge", true);
        request(pub.message({}),
                Requested{shared_from_this(), std::move(handler)});
    }

    void safePublish(Pub&& p, CompletionHandler<PublicationId>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Pub p;
            CompletionHandler<PublicationId> f;
            void operator()() {self->publish(std::move(p), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(p), std::move(f));
    }

    void enroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                CompletionHandler<Registration>&& f)
    {
        struct Requested
        {
            Ptr self;
            ProcedureRegistration r;
            CompletionHandler<Registration> f;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (!me.checkReply(reply, WampMsgType::registered, f))
                    return;
                auto& msg = messageCast<RegisteredMessage>(*reply);
                auto reg = me.registry_.enroll(std::move(r), self, msg);
                me.completeNow(f, std::move(reg));
            }
        };

        if (!checkState(State::established, f))
            return;

        ProcedureRegistration reg{std::move(c), std::move(i)};
        request(p.message({}),
                Requested{shared_from_this(), std::move(reg), std::move(f)});
    }

    void safeEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                    CompletionHandler<Registration>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Procedure p;
            CallSlot c;
            InterruptSlot i;
            CompletionHandler<Registration> f;

            void operator()()
            {
                self->enroll(std::move(p), std::move(c), std::move(i),
                             std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(p), std::move(c), std::move(i),
                                   std::move(f));
    }

    void enroll(Stream&& s, StreamSlot&& ss,
                CompletionHandler<Registration>&& f)
    {
        struct Requested
        {
            Ptr self;
            StreamRegistration r;
            CompletionHandler<Registration> f;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (!me.checkReply(reply, WampMsgType::registered, f))
                    return;
                auto& msg = messageCast<RegisteredMessage>(*reply);
                auto reg = me.registry_.enroll(std::move(r), self, msg);
                me.completeNow(f, std::move(reg));
            }
        };

        if (!checkState(State::established, f))
            return;

        StreamRegistration reg{std::move(ss), s.invitationExpected()};
        request(s.message({}),
                Requested{shared_from_this(), std::move(reg), std::move(f)});
    }

    void safeEnroll(Stream&& s, StreamSlot&& ss,
                    CompletionHandler<Registration>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Stream s;
            StreamSlot ss;
            CompletionHandler<Registration> f;

            void operator()()
            {
                self->enroll(std::move(s), std::move(ss), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(ss), std::move(f));
    }

    void unregister(const Registration& reg) override
    {
        struct Requested
        {
            Ptr self;

            void operator()(ErrorOr<Message> reply)
            {
                // Don't propagate WAMP errors, as we prefer this
                // to be a no-fail cleanup operation.
                self->checkReplyWithoutHandler(reply, WampMsgType::unregistered);
            }
        };

        if (registry_.unregister(reg) && state() == State::established)
        {
            UnregisterMessage msg(reg.id());
            request(msg, Requested{shared_from_this()});
        }
    }

    void safeUnregister(const Registration& r) override
    {
        struct Dispatched
        {
            Ptr self;
            Registration r;
            void operator()() {self->unregister(r);}
        };

        safelyDispatch<Dispatched>(r);
    }

    void unregister(const Registration& reg, CompletionHandler<bool>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<bool> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, WampMsgType::unregistered, handler))
                    me.completeNow(handler, true);
            }
        };

        if (registry_.unregister(reg))
        {
            UnregisterMessage msg(reg.id());
            if (checkState(State::established, handler))
                request(msg, Requested{shared_from_this(), std::move(handler)});
        }
        else
        {
            complete(handler, false);
        }
    }

    void safeUnregister(const Registration& r, CompletionHandler<bool>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Registration r;
            CompletionHandler<bool> f;
            void operator()() {self->unregister(std::move(r), std::move(f));}
        };

        safelyDispatch<Dispatched>(r, std::move(f));
    }

    void call(Rpc&& rpc, CompletionHandler<Result>&& handler)
    {
        struct Requested
        {
            Ptr self;
            Error* errorPtr;
            CompletionHandler<Result> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, WampMsgType::result, handler,
                                  errorPtr))
                {
                    auto& msg = messageCast<ResultMessage>(*reply);
                    me.completeNow(handler, Result({}, std::move(msg)));
                }
            }
        };

        if (!checkState(State::established, handler))
            return;

        auto boundCancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = request(
            rpc.message({}),
            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
        if (!requestId)
            return;

        auto& rpcCancelSlot = rpc.cancellationSlot({});
        if (rpcCancelSlot.is_connected())
        {
            rpcCancelSlot.emplace(shared_from_this(), *requestId);
        }
        else if (boundCancelSlot.is_connected())
        {
            auto self = shared_from_this();
            auto reqId = *requestId;
            auto mode = rpc.cancelMode();
            boundCancelSlot.assign(
                [this, self, reqId, mode](boost::asio::cancellation_type_t)
                {
                    safeCancelCall(reqId, mode);
                });
        }

        if (rpc.callerTimeout().count() != 0)
            timeoutScheduler_->insert(*requestId, rpc.callerTimeout());
    }

    void safeCall(Rpc&& r, CompletionHandler<Result>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Rpc r;
            CompletionHandler<Result> f;

            void operator()()
            {
                self->call(std::move(r), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(r), std::move(f));
    }

    ErrorOrDone cancelCall(RequestId reqId, CallCancelMode mode) override
    {
        if (state() != State::established)
            return makeUnexpectedError(Errc::invalidState);

        CallCancellation cancellation{reqId, mode};
        bool found = requestor_.cancelCall(cancellation);

        // Always send the CANCEL message in all modes if a matching
        // call was found.
        if (found)
        {
            auto sent = peer_.send(cancellation.message({}));
            if (!sent)
                return UnexpectedError(sent.error());
        }
        return found;
    }

    FutureErrorOrDone safeCancelCall(RequestId r, CallCancelMode m) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            CallCancelMode m;
            ErrorOrDonePromise p;

            void operator()()
            {
                try
                {
                    p.set_value(self->cancelCall(r, m));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(r, m, std::move(p));
        return fut;
    }

    void requestStream(StreamRequest&& inv, ChunkSlot&& onChunk,
                       CompletionHandler<CallerChannel>&& handler)
    {
        if (!checkState(State::established, handler))
            return;

        auto channel = requestor_.requestStream(
            true, shared_from_this(), std::move(inv), std::move(onChunk),
            std::move(handler));

        if (channel && inv.callerTimeout().count() != 0)
            timeoutScheduler_->insert(channel->id(), inv.callerTimeout());
    }

    void safeRequestStream(StreamRequest&& i, ChunkSlot&& c,
                           CompletionHandler<CallerChannel>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            StreamRequest i;
            ChunkSlot c;
            CompletionHandler<CallerChannel> f;

            void operator()()
            {
                self->requestStream(std::move(i), std::move(c), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(i), std::move(c), std::move(f));
    }

    ErrorOr<CallerChannel> openStream(StreamRequest&& req, ChunkSlot&& onChunk)
    {
        if (state() != State::established)
            return makeUnexpectedError(Errc::invalidState);

        auto channel = requestor_.requestStream(
            false, shared_from_this(), std::move(req), std::move(onChunk));

        if (channel && req.callerTimeout().count() != 0)
            timeoutScheduler_->insert(channel->id(), req.callerTimeout());

        return channel;
    }

    std::future<ErrorOr<CallerChannel>> safeOpenStream(StreamRequest&& r,
                                                       ChunkSlot c)
    {
        struct Dispatched
        {
            Ptr self;
            StreamRequest r;
            ChunkSlot c;
            std::promise<ErrorOr<CallerChannel>> p;

            void operator()()
            {
                try
                {
                    p.set_value(self->openStream(std::move(r), std::move(c)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        std::promise<ErrorOr<CallerChannel>> p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(std::move(r), std::move(c), std::move(p));
        return fut;
    }

    ErrorOrDone sendCallerChunk(RequestId reqId,
                                CallerOutputChunk chunk) override
    {
        return requestor_.sendCallerChunk(reqId, std::move(chunk));
    }

    FutureErrorOrDone safeSendCallerChunk(RequestId r,
                                          CallerOutputChunk c) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            CallerOutputChunk c;
            ErrorOrDonePromise p;

            void operator()()
            {
                try
                {
                    p.set_value(self->sendCallerChunk(r, std::move(c)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(r, std::move(c), std::move(p));
        return fut;
    }

    FutureErrorOrDone safeCancelStream(RequestId r) override
    {
        // TODO: Check that router supports call cancellation
        return safeCancelCall(r, CallCancelMode::killNoWait);
    }

    ErrorOrDone sendCalleeChunk(RequestId reqId,
                                CalleeOutputChunk&& chunk) override
    {
        if (state() != State::established)
            return makeUnexpectedError(Errc::invalidState);
        return registry_.yield(reqId, std::move(chunk));
    }

    FutureErrorOrDone safeSendCalleeChunk(RequestId r,
                                          CalleeOutputChunk&& c) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            CalleeOutputChunk c;
            ErrorOrDonePromise p;

            void operator()()
            {
                try
                {
                    p.set_value(self->sendCalleeChunk(r, std::move(c)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(r, std::move(c), std::move(p));
        return fut;
    }

    ErrorOrDone yield(RequestId reqId, Result&& result) override
    {
        if (state() != State::established)
            return makeUnexpectedError(Errc::invalidState);
        return registry_.yield(reqId, std::move(result));
    }

    FutureErrorOrDone safeYield(RequestId i, Result&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId i;
            Result r;
            ErrorOrDonePromise p;

            void operator()()
            {
                try
                {
                    p.set_value(self->yield(i, std::move(r)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(i, std::move(r), std::move(p));
        return fut;
    }

    ErrorOrDone yield(RequestId reqId, Error&& error) override
    {
        if (state() != State::established)
            return makeUnexpectedError(Errc::invalidState);
        return registry_.yield(reqId, std::move(error));
    }

    FutureErrorOrDone safeYield(RequestId r, Error&& e) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            Error e;
            ErrorOrDonePromise p;

            void operator()()
            {
                try
                {
                    p.set_value(self->yield(r, std::move(e)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(r, std::move(e), std::move(p));
        return fut;
    }

private:
    using ErrorOrDonePromise  = std::promise<ErrorOrDone>;

    using CallerTimeoutDuration  = typename Rpc::TimeoutDuration;
    using CallerTimeoutScheduler = TimeoutScheduler<RequestId>;
    using Message                = WampMessage;
    using RequestKey             = typename Message::RequestKey;
    using RequestHandler         = AnyCompletionHandler<void (ErrorOr<Message>)>;

    static void outputErrorDetails(std::ostream& out, const Error& e)
    {
        out << "ERROR with URI=" << e.uri();
        if (!e.args().empty())
            out << ", with Args=" << e.args();
        if (!e.kwargs().empty())
            out << ", with ArgsKv=" << e.kwargs();
    }

    Client(const AnyIoExecutor& exec, AnyCompletionExecutor userExec)
        : peer_(false),
          executor_(std::move(exec)),
          userExecutor_(std::move(userExec)),
          strand_(boost::asio::make_strand(executor_)),
          readership_(executor_, userExecutor_),
          registry_(peer_, executor_, userExecutor_),
          requestor_(peer_, strand_, executor_, userExecutor_),
          timeoutScheduler_(CallerTimeoutScheduler::create(strand_))
    {
        peer_.setInboundMessageHandler(
            [this](Message msg) {onInbound(std::move(msg));} );
        peer_.setLogHandler(
            [this](LogEntry entry) {onLog(std::move(entry));} );
        peer_.setStateChangeHandler(
            [this](State s, std::error_code ec) {onStateChanged(s, ec);});
        timeoutScheduler_->listen(
            [this](RequestId reqId)
            {
                cancelCall(reqId, CallCancelMode::killNoWait);
            });
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand_, F{shared_from_this(), std::forward<Ts>(args)...});
    }

    template <typename F>
    bool checkState(State expectedState, F& handler)
    {
        // TODO: std::atomic::compare_exchange_strong
        bool valid = state() == expectedState;
        if (!valid)
        {
            auto unex = makeUnexpectedError(Errc::invalidState);
            if (!isTerminating_)
            {
                postVia(executor_, userExecutor_, std::move(handler),
                        std::move(unex));
            }
        }
        return valid;
    }

    void onLog(LogEntry&& entry)
    {
        if (logHandler_)
            dispatchHandler(logHandler_, std::move(entry));
    }

    void onStateChanged(SessionState s, std::error_code ec)
    {
        if (ec)
            abortPending(ec);
        if (stateChangeHandler_)
            postHandler(stateChangeHandler_, s, ec);
    }

    ErrorOr<RequestId> request(Message& msg, RequestHandler&& handler)
    {
        return requestor_.request(msg, std::move(handler));
    }

    void abortPending(std::error_code ec)
    {
        if (isTerminating_)
            requestor_.clear();
        else
            requestor_.abandonAll(ec);
    }

    template <typename TErrc>
    void abortPending(TErrc errc) {abortPending(make_error_code(errc));}

    void doConnect(ConnectionWishList&& wishes, size_t index,
                   std::shared_ptr<CompletionHandler<size_t>> handler)
    {
        struct Established
        {
            std::weak_ptr<Client> self;
            ConnectionWishList wishes;
            size_t index;
            std::shared_ptr<CompletionHandler<size_t>> handler;

            void operator()(ErrorOr<Transporting::Ptr> transport)
            {
                auto locked = self.lock();
                if (!locked)
                    return;

                auto& me = *locked;
                if (me.isTerminating_)
                    return;

                if (!transport)
                {
                    me.onConnectFailure(std::move(wishes), index,
                                        transport.error(), std::move(handler));
                }
                else if (me.state() == State::connecting)
                {
                    auto codec = wishes.at(index).makeCodec();
                    me.peer_.connect(std::move(*transport), std::move(codec));
                    me.completeNow(*handler, index);
                }
                else
                {
                    auto ec = make_error_code(TransportErrc::aborted);
                    me.completeNow(*handler, UnexpectedError(ec));
                }
            }
        };

        currentConnector_ = wishes.at(index).makeConnector(strand_);
        currentConnector_->establish(
            Established{shared_from_this(), std::move(wishes), index,
                        std::move(handler)});
    }

    void onConnectFailure(ConnectionWishList&& wishes, size_t index,
                          std::error_code ec,
                          std::shared_ptr<CompletionHandler<size_t>> handler)
    {
        if (ec == TransportErrc::aborted)
        {
            completeNow(*handler, UnexpectedError(ec));
        }
        else
        {
            auto newIndex = index + 1;
            if (newIndex < wishes.size())
            {
                doConnect(std::move(wishes), newIndex, std::move(handler));
            }
            else
            {
                if (wishes.size() > 1)
                    ec = make_error_code(TransportErrc::exhausted);
                peer_.failConnecting(ec);
                completeNow(*handler, UnexpectedError(ec));
            }
        }
    }

    void clear()
    {
        readership_.clear();
        registry_.clear();
        timeoutScheduler_->clear();
    }

    void sendUnsubscribe(SubscriptionId subId)
    {
        struct Requested
        {
            Ptr self;

            void operator()(ErrorOr<Message> reply)
            {
                // Don't propagate WAMP errors, as we prefer
                // this to be a no-fail cleanup operation.
                self->checkReplyWithoutHandler(reply, WampMsgType::unsubscribed);
            }
        };

        if (state() == State::established)
        {
            UnsubscribeMessage msg(subId);
            request(msg, Requested{shared_from_this()});
        }
    }

    void sendUnsubscribe(SubscriptionId subId,
                         CompletionHandler<bool>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<bool> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, WampMsgType::unsubscribed, handler))
                {
                    me.completeNow(handler, true);
                }
            }
        };

        if (checkState(State::established, handler))
        {
            UnsubscribeMessage msg(subId);
            request(msg, Requested{shared_from_this(), std::move(handler)});
        }
    }

    void onWelcome(CompletionHandler<Welcome>&& handler, Message& reply,
                   String&& realmUri)
    {
        auto& welcomeMsg = messageCast<WelcomeMessage>(reply);
        Welcome info{{}, std::move(realmUri), std::move(welcomeMsg)};
        completeNow(handler, std::move(info));
    }

    void onJoinAborted(CompletionHandler<Welcome>&& handler, Message& reply,
                       Reason* abortPtr)
    {
        auto& abortMsg = messageCast<AbortMessage>(reply);
        const auto& uri = abortMsg.uri();
        WampErrc errc = errorUriToCode(uri);
        const auto& details = reply.as<Object>(1);

        if (abortPtr != nullptr)
        {
            *abortPtr = Reason({}, std::move(abortMsg));
        }
        else if ((logLevel() <= LogLevel::error) &&
                 (errc == WampErrc::unknown || !details.empty()))
        {
            std::ostringstream oss;
            oss << "JOIN request aborted by peer with error URI=" << uri;
            if (!abortMsg.options().empty())
                oss << ", Details=" << abortMsg.options();
            log(LogLevel::error, oss.str());
        }

        completeNow(handler, makeUnexpectedError(errc));
    }

    void onInbound(Message msg)
    {
        switch (msg.type())
        {
        case WampMsgType::challenge:  return onChallenge(msg);
        case WampMsgType::event:      return onEvent(msg);
        case WampMsgType::invocation: return onInvocation(msg);
        case WampMsgType::interrupt:  return onInterrupt(msg);
        default:                      return onWampReply(msg);
        }
    }

    void onChallenge(Message& msg)
    {
        auto& challengeMsg = messageCast<ChallengeMessage>(msg);
        Challenge challenge({}, shared_from_this(), std::move(challengeMsg));

        if (challengeHandler_)
        {
            dispatchHandler(challengeHandler_, std::move(challenge));
        }
        else
        {
            if (logLevel() <= LogLevel::error)
            {
                std::ostringstream oss;
                oss << "Received a CHALLENGE with no registered handler, "
                       "with method " << challenge.method();
                log(LogLevel::error, oss.str());
            }

            // Send empty signature to avoid deadlock with other peer.
            authenticate(Authentication(""));
        }
    }

    void onEvent(Message& msg)
    {
        auto& eventMsg = messageCast<EventMessage>(msg);
        bool ok = readership_.onEvent(eventMsg, shared_from_this());
        if (!ok && logLevel() <= LogLevel::warning)
        {
            std::ostringstream oss;
            oss << "Discarding an EVENT that is not subscribed to "
                   "(with subId=" << eventMsg.subscriptionId()
                << " pubId=" << eventMsg.publicationId() << ")";
            log(LogLevel::warning, oss.str());
        }
    }

    void onInvocation(Message& msg)
    {
        auto& invMsg = messageCast<InvocationMessage>(msg);
        auto regId = invMsg.registrationId();

        auto errc = registry_.onInvocation(invMsg, shared_from_this());
        if (errc == WampErrc::noSuchProcedure)
        {
            log(LogLevel::error,
                "No matching procedure for INVOCATION with registration ID "
                    + std::to_string(regId));
        }
        if (errc == WampErrc::protocolViolation)
        {
            peer_.abort(
                Reason(WampErrc::protocolViolation)
                    .withHint("Router attempted to reinvoke a pending RPC "
                              "that is closed to further progress"));
        }
    }

    void onInterrupt(Message& msg)
    {
        auto& intrMsg = messageCast<InterruptMessage>(msg);
        registry_.onInterrupt(intrMsg, shared_from_this());
    }

    void onWampReply(Message& msg)
    {
        // TODO: Bump timeout timer if progressive result
        const char* msgName = msg.name();
        assert(msg.isReply());
        if (!requestor_.onReply(std::move(msg)))
        {
            log(LogLevel::warning,
                std::string("Discarding received ") + msgName +
                " message with no matching request");
        }
    }

    template <typename THandler>
    bool checkError(const ErrorOr<Message>& msg, THandler& handler)
    {
        bool ok = msg.has_value();
        if (!ok)
            dispatchHandler(handler, UnexpectedError(msg.error()));
        return ok;
    }

    template <typename THandler>
    bool checkReply(ErrorOr<Message>& reply, WampMsgType type,
                    THandler& handler, Error* errorPtr = nullptr)
    {
        if (!checkError(reply, handler))
            return false;

        if (reply->type() != WampMsgType::error)
        {
            assert((reply->type() == type) &&
                   "Unexpected WAMP message type");
            return true;
        }

        auto& errMsg = messageCast<ErrorMessage>(*reply);
        Error error({}, std::move(errMsg));
        WampErrc errc = error.errorCode();

        if (errorPtr != nullptr)
            *errorPtr = std::move(error);
        else
            logErrorReplyIfNeeded(error, errc, type);

        dispatchHandler(handler, makeUnexpectedError(errc));
        return false;
    }

    void logErrorReplyIfNeeded(const Error& error, WampErrc errc,
                               WampMsgType reqType)
    {
        // Only log if there is extra error information that cannot
        // passed to the handler via an error code.
        if (logLevel() > LogLevel::error)
            return;
        if ((errc != WampErrc::unknown) && !error.hasArgs())
            return;
        std::ostringstream oss;
        oss << "Expected " << MessageTraits::lookup(reqType).name
            << " reply but got ";
        outputErrorDetails(oss, error);
        log(LogLevel::error, oss.str());
    }

    void checkReplyWithoutHandler(ErrorOr<Message>& reply, WampMsgType type)
    {
        std::string msgTypeName(MessageTraits::lookup(type).name);
        if (!reply.has_value())
        {
            if (logLevel() <= LogLevel::error)
            {
                log(LogLevel::error,
                    "Failure receiving reply for " + msgTypeName + " message",
                    reply.error());
            }
        }
        else if (reply->type() == WampMsgType::error)
        {
            if (logLevel() <= LogLevel::error)
            {
                auto& msg = messageCast<ErrorMessage>(*reply);
                Error error({}, std::move(msg));
                std::ostringstream oss;
                oss << "Expected reply for " << msgTypeName
                    << " message but got ";
                outputErrorDetails(oss, error);
                log(LogLevel::error, oss.str());
            }
        }
        else
        {
            assert((reply->type() == type) && "Unexpected WAMP message type");
        }
    }

    LogLevel logLevel() const {return peer_.logLevel();}

    void log(LogLevel severity, std::string message, std::error_code ec = {})
    {
        peer_.log(severity, std::move(message), ec);
    }

    template <typename S, typename... Ts>
    void dispatchHandler(AnyCompletionHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        dispatchVia(executor_, userExecutor_, std::move(f),
                    std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void dispatchHandler(const AnyReusableHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        dispatchVia(executor_, userExecutor_, f, std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void postHandler(const AnyReusableHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        postVia(executor_, userExecutor_, f, std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void complete(AnyCompletionHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        postVia(executor_, userExecutor_, std::move(f),
                std::forward<Ts>(args)...);
    }

    template <typename F, typename... Ts>
    void completeRequest(F& handler, Ts&&... args)
    {
        boost::asio::post(
            strand_,
            std::bind(std::move(handler), std::forward<Ts>(args)...));
    }

    template <typename S, typename... Ts>
    void completeNow(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        dispatchHandler(handler, std::forward<Ts>(args)...);
    }

    Peer peer_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    IoStrand strand_;
    Connecting::Ptr currentConnector_;
    Readership readership_;
    ProcedureRegistry registry_;
    Requestor requestor_;
    CallerTimeoutScheduler::Ptr timeoutScheduler_;
    LogHandler logHandler_;
    StateChangeHandler stateChangeHandler_;
    ChallengeHandler challengeHandler_;
    bool isTerminating_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
