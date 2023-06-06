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
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <boost/asio/post.hpp>
#include "../any.hpp"
#include "../anyhandler.hpp"
#include "../calleestreaming.hpp"
#include "../callerstreaming.hpp"
#include "../codec.hpp"
#include "../cancellation.hpp"
#include "../connector.hpp"
#include "../errorinfo.hpp"
#include "../features.hpp"
#include "../pubsubinfo.hpp"
#include "../registration.hpp"
#include "../rpcinfo.hpp"
#include "../sessioninfo.hpp"
#include "../subscription.hpp"
#include "../traits.hpp"
#include "../transport.hpp"
#include "../version.hpp"
#include "clientcontext.hpp"
#include "commandinfo.hpp"
#include "matchuri.hpp"
#include "streamchannel.hpp"
#include "peer.hpp"
#include "timeoutscheduler.hpp"
#include "trackedslot.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct StreamRegistration
{
    AnyReusableHandler<void (CalleeChannel)> streamSlot;
    Uri uri;
    RegistrationId registrationId;
    bool invitationExpected;
};

//------------------------------------------------------------------------------
class StreamRecord
{
public:
    using TimeoutDuration = Rpc::TimeoutDuration;

    using CompletionHandler =
        AnyCompletionHandler<void (ErrorOr<CallerChannel>)>;

    explicit StreamRecord(CallerChannelImpl::Ptr c, StreamRequest& i,
                          CompletionHandler&& f = {})
        : handler_(std::move(f)),
          weakChannel_(c),
          errorPtr_(i.error({})),
          timeout_(i.callerTimeout())
    {
        if (handler_)
            channel_ = std::move(c);
    }

    void onReply(Message&& msg, AnyIoExecutor& exec)
    {
        if (msg.kind() == MessageKind::result)
            onResult(std::move(msg), exec);
        else
            onError(std::move(msg), exec);
    }

    void cancel(AnyIoExecutor& exec, WampErrc errc)
    {
        abandon(makeUnexpectedError(errc), exec);
    }

    void abandon(UnexpectedError unex, AnyIoExecutor& exec)
    {
        if (handler_)
            postAny(exec, std::move(handler_), unex);
        else if (channel_)
            channel_->abandon(unex);
        else if (auto ch = weakChannel_.lock())
            ch->abandon(unex);

        handler_ = nullptr;
        channel_.reset();
        weakChannel_.reset();
    }

    bool hasTimeout() const {return timeout_.count() != 0;}

    TimeoutDuration timeout() const {return timeout_;}

private:
    explicit StreamRecord(CallerChannelImpl::Ptr c, Error* e,
                          CompletionHandler&& f)
        : handler_(std::move(f)),
          channel_(std::move(c)),
          weakChannel_(channel_),
          errorPtr_(e)
    {}

    void onResult(Message&& msg, AnyIoExecutor& exec)
    {
        if (channel_)
        {
            if (channel_->expectsRsvp())
                channel_->setRsvp(std::move(msg));

            if (handler_)
            {
                dispatchAny(exec, std::move(handler_),
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

    void onError(Message&& msg, AnyIoExecutor& exec)
    {
        if (channel_)
        {
            if (handler_)
            {
                Error error{{}, std::move(msg)};
                auto unex = makeUnexpectedError(error.errorCode());
                if (errorPtr_)
                    *errorPtr_ = std::move(error);
                dispatchAny(exec, std::move(handler_), unex);
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
    TimeoutDuration timeout_ = {};
};

//------------------------------------------------------------------------------
class Requestor
{
public:
    using TimeoutDuration = typename Rpc::TimeoutDuration;
    using RequestHandler = AnyCompletionHandler<void (ErrorOr<Message>)>;
    using StreamRequestHandler =
        AnyCompletionHandler<void (ErrorOr<CallerChannel>)>;
    using ChunkSlot = AnyReusableHandler<void (CallerChannel,
                                               ErrorOr<CallerInputChunk>)>;

    Requestor(Peer& peer, IoStrand strand, AnyIoExecutor exec,
              AnyCompletionExecutor fallbackExec)
        : deadlines_(CallerTimeoutScheduler::create(strand)),
          strand_(std::move(strand)),
          executor_(std::move(exec)),
          fallbackExecutor_(std::move(fallbackExec)),
          peer_(peer)
    {
        deadlines_->listen(
            [this](RequestId reqId)
            {
                cancelCall(reqId, CallCancelMode::killNoWait,
                           WampErrc::timeout);
            });
    }

    template <typename C>
    ErrorOr<RequestId> request(C&& command, RequestHandler&& handler)
    {
        return request(std::move(command), TimeoutDuration{0},
                       std::move(handler));
    }

    template <typename C>
    ErrorOr<RequestId> request(C&& command, TimeoutDuration timeout,
                               RequestHandler&& handler)
    {

        using HasRequestId = MetaBool<ValueTypeOf<C>::hasRequestId({})>;
        return doRequest(HasRequestId{}, command, timeout, std::move(handler));
    }

    ErrorOr<CallerChannel> requestStream(
        bool rsvpExpected, ClientContext caller, StreamRequest&& req,
        ChunkSlot&& onChunk, StreamRequestHandler&& handler = {})
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        assert(nextRequestId_ < 9007199254740992u);
        ChannelId channelId = nextRequestId_ + 1;
        auto uri = req.uri();
        req.setRequestId({}, channelId);

        auto sent = peer_.send(std::move(req));
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
            fallbackExecutor_);
        auto emplaced = channels_.emplace(
            channelId, StreamRecord{channel, req, std::move(handler)});
        assert(emplaced.second);

        if (req.callerTimeout().count() != 0)
            deadlines_->insert(channel->id(), req.callerTimeout());

        return CallerChannel{{}, std::move(channel)};
    }

    bool onReply(Message&& msg) // Returns true if request was found
    {
        assert(msg.isReply());
        auto key = msg.requestKey();

        {
            auto kv = requests_.find(key);
            if (kv != requests_.end())
            {
                auto handler = std::move(kv->second);
                requests_.erase(kv);
                if (key.first == MessageKind::call)
                    deadlines_->erase(key.second);
                completeRequest(handler, std::move(msg));
                return true;
            }
        }

        {
            if (key.first != MessageKind::call)
                return false;
            auto kv = channels_.find(key.second);
            if (kv == channels_.end())
                return false;

            if (msg.isProgress())
            {
                StreamRecord& rec = kv->second;
                rec.onReply(std::move(msg), executor_);
                deadlines_->update(key.second, rec.timeout());
            }
            else
            {
                StreamRecord rec{std::move(kv->second)};
                deadlines_->erase(key.second);
                channels_.erase(kv);
                rec.onReply(std::move(msg), executor_);
            }
        }

        return true;
    }

    // Returns true if request was found
    ErrorOrDone cancelCall(RequestId requestId, CallCancelMode mode,
                           WampErrc errc = WampErrc::cancelled)
    {
        // If the cancel mode is not 'kill', don't wait for the router's
        // ERROR message and post the request handler immediately
        // with a WampErrc::cancelled error code.

        auto unex = makeUnexpectedError(errc);

        {
            RequestKey key{MessageKind::call, requestId};
            auto kv = requests_.find(key);
            if (kv != requests_.end())
            {
                deadlines_->erase(requestId);
                if (mode != CallCancelMode::kill)
                {
                    auto handler = std::move(kv->second);
                    requests_.erase(kv);
                    completeRequest(handler, unex);
                }
                return peer_.send(CallCancellation{requestId, mode});
            }
        }

        {
            auto kv = channels_.find(requestId);
            if (kv == channels_.end())
                return false;

            deadlines_->erase(requestId);
            if (mode != CallCancelMode::kill)
            {
                StreamRecord req{std::move(kv->second)};
                channels_.erase(kv);
                req.cancel(executor_, errc);
            }
            return peer_.send(CallCancellation{requestId, mode});
        }

        return false;
    }

    ErrorOrDone sendCallerChunk(CallerOutputChunk&& chunk)
    {
        auto key = chunk.requestKey({});
        if (requests_.count(key) == 0 && channels_.count(key.second) == 0)
            return false;
        return peer_.send(std::move(chunk));
    }

    void abandonAll(std::error_code ec)
    {
        UnexpectedError unex{ec};
        for (auto& kv: requests_)
            completeRequest(kv.second, unex);
        for (auto& kv: channels_)
            kv.second.abandon(unex, executor_);
        clear();
    }

    void clear()
    {
        deadlines_->clear();
        requests_.clear();
        channels_.clear();
        nextRequestId_ = nullId();
    }

private:
    using RequestKey = typename Message::RequestKey;
    using CallerTimeoutScheduler = TimeoutScheduler<RequestId>;

    template <typename C>
    ErrorOr<RequestId> doRequest(TrueType, C& command, TimeoutDuration timeout,
                                 RequestHandler&& handler)
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        assert(nextRequestId_ < 9007199254740992u);
        RequestId requestId = nextRequestId_ + 1;
        command.setRequestId({}, requestId);

        auto sent = peer_.send(std::move(command));
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        ++nextRequestId_;
        if (!handler)
            return requestId;

        auto emplaced = requests_.emplace(command.requestKey({}),
                                          std::move(handler));
        assert(emplaced.second);

        if (timeout.count() != 0)
            deadlines_->insert(requestId, timeout);

        return requestId;
    }

    template <typename C>
    ErrorOr<RequestId> doRequest(FalseType, C& command, TimeoutDuration timeout,
                                 RequestHandler&& handler)
    {
        RequestId requestId = nullId();

        auto sent = peer_.send(std::move(command));
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        if (!handler)
            return requestId;

        auto emplaced = requests_.emplace(command.requestKey({}),
                                          std::move(handler));
        assert(emplaced.second);

        if (timeout.count() != 0)
            deadlines_->insert(requestId, timeout);

        return requestId;
    }

    template <typename F, typename... Ts>
    void completeRequest(F& handler, Ts&&... args)
    {
        if (!handler)
            return;
        boost::asio::post(
            strand_,
            std::bind(std::move(handler), std::forward<Ts>(args)...));
    }

    std::map<RequestKey, RequestHandler> requests_;
    std::map<ChannelId, StreamRecord> channels_;
    CallerTimeoutScheduler::Ptr deadlines_;
    IoStrand strand_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor fallbackExecutor_;
    Peer& peer_;
    RequestId nextRequestId_ = nullId();
};


//------------------------------------------------------------------------------
class Readership
{
public:
    using EventSlotKey = ClientLike::EventSlotKey;
    using EventSlot = AnyReusableHandler<void (Event)>;

    Readership(AnyIoExecutor exec) : executor_(std::move(exec)) {}

    Subscription subscribe(MatchUri topic, EventSlot& handler,
                           ClientContext subscriber)
    {
        assert(topic.policy() != MatchPolicy::unknown);
        auto kv = byTopic_.find(topic);
        if (kv == byTopic_.end())
            return {};

        return addSlotToExisingSubscription(kv->second, std::move(handler),
                                            std::move(subscriber));
    }

    Subscription onSubscribed(SubscriptionId subId, MatchUri topic,
                              EventSlot&& handler, ClientContext subscriber)
    {
        // Check if the router treats the topic as belonging to an existing
        // subcription.
        auto kv = subscriptions_.find(subId);
        if (kv != subscriptions_.end())
        {
            return addSlotToExisingSubscription(kv, std::move(handler),
                                                std::move(subscriber));
        }

        auto slotId = nextSlotId();
        auto trackedSlot = TrackedSlotType::create({subId, slotId},
                                                   std::move(handler), subscriber);
        auto emplaced =
            subscriptions_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(subId),
                std::forward_as_tuple(topic, slotId, trackedSlot));
        assert(emplaced.second);

        auto emplaced2 = byTopic_.emplace(std::move(topic), emplaced.first);
        assert(emplaced2.second);

        return Subscription({}, std::move(trackedSlot));
    }

    // Returns true if the last local slot was removed from a subscription
    // and the client needs to send an UNSUBSCRIBE message.
    bool unsubscribe(EventSlotKey key)
    {
        auto subId = key.first;
        auto kv = subscriptions_.find(subId);
        if (kv == subscriptions_.end())
            return false;

        auto slotId = key.second;
        auto& record = kv->second;
        record.slots.erase(slotId);
        if (!record.slots.empty())
            return false;

        byTopic_.erase(record.topic);
        subscriptions_.erase(kv);
        return true;
    }

    // Returns true if there are any subscriptions matching the event
    bool onEvent(const Event& event, ClientContext subscriber)
    {
        auto found = subscriptions_.find(event.subscriptionId());
        if (found == subscriptions_.end())
            return false;

        const auto& record = found->second;
        assert(!record.slots.empty());
        for (const auto& kv: record.slots)
        {
            const auto& slot = kv.second;
            postEvent(slot, event, subscriber);
        }
        return true;
    }

    const Uri& lookupTopicUri(SubscriptionId subId)
    {
        static const Uri empty;
        auto found = subscriptions_.find(subId);
        if (found == subscriptions_.end())
            return empty;
        return found->second.topic.uri();
    }

    void clear()
    {
        byTopic_.clear();
        subscriptions_.clear();
        nextSlotId_ = nullId();
    }

private:
    using SlotId = ClientLike::SlotId;

    using TrackedSlotType = TrackedSlot<EventSlotTag, EventSlotKey,
                                        void (Event)>;

    struct Record
    {
        Record(MatchUri topic, SlotId slotId, TrackedSlotType::Ptr trackedSlot)
            : topic(std::move(topic))
        {
            slots.emplace(slotId, std::move(trackedSlot));
        }

        std::map<SlotId, TrackedSlotType::Ptr> slots;
        MatchUri topic;
    };

    using SubscriptionMap = std::map<SubscriptionId, Record>;
    using ByTopic = std::map<MatchUri, SubscriptionMap::iterator>;

    Subscription addSlotToExisingSubscription(SubscriptionMap::iterator iter,
                                              EventSlot&& handler,
                                              ClientContext&& subscriber)
    {
        auto subId = iter->first;
        auto& record = iter->second;
        auto slotId = nextSlotId();
        auto slot = TrackedSlotType::create({subId, slotId},
                                            std::move(handler), subscriber);
        auto emplaced = record.slots.emplace(slotId, slot);
        assert(emplaced.second);
        return Subscription{{}, std::move(slot)};
    }

    void postEvent(TrackedSlotType::Ptr slot, Event event,
                   ClientContext subscriber)
    {
        struct Posted
        {
            ClientContext subscriber;
            Event event;
            TrackedSlotType::Ptr slot;

            void operator()()
            {
                // Copy the publication ID before the Event object
                // gets moved away.
                auto pubId = event.publicationId();

                // The catch clauses are to prevent the publisher crashing
                // subscribers when it passes arguments having incorrect type.
                try
                {
                    assert(event.ready());
                    if (slot->armed())
                        slot->invoke(std::move(event));
                }
                catch (Error& error)
                {
                    auto subId = slot->key().first;
                    error["subscriptionId"] = subId;
                    error["publicationId"] = pubId;
                    subscriber.onEventError(std::move(error), subId);
                }
                catch (const error::BadType& e)
                {
                    Error error(e);
                    auto subId = slot->key().first;
                    error["subscriptionId"] = subId;
                    error["publicationId"] = pubId;
                    subscriber.onEventError(std::move(error), subId);
                }
            }
        };

        auto slotExec = slot->executor();
        event.setExecutor({}, slotExec);
        Posted posted{subscriber, std::move(event), std::move(slot)};
        boost::asio::post(
            executor_,
            boost::asio::bind_executor(slotExec, std::move(posted)));
    }

    SlotId nextSlotId() {return nextSlotId_++;}

    SubscriptionMap subscriptions_;
    ByTopic byTopic_;
    AnyIoExecutor executor_;
    SlotId nextSlotId_ = 0;
};


//------------------------------------------------------------------------------
struct ProcedureRegistration
{
    using CallSlot = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;

    CallSlot callSlot;
    InterruptSlot interruptSlot;
    Uri uri;
    RegistrationId registrationId;
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
    using CallSlot      = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;
    using StreamSlot    = AnyReusableHandler<void (CalleeChannel)>;

    ProcedureRegistry(Peer& peer, AnyIoExecutor exec)
        : executor_(std::move(exec)),
          peer_(peer)
    {}

    ErrorOr<Registration> enroll(ProcedureRegistration&& reg,
                                 ClientContext callee)
    {
        auto regId = reg.registrationId;
        auto emplaced = procedures_.emplace(regId, std::move(reg));
        if (!emplaced.second)
            return makeUnexpectedError(WampErrc::procedureAlreadyExists);
        return Registration{{}, callee, regId};
    }

    ErrorOr<Registration> enroll(StreamRegistration&& reg,
                                 ClientContext callee)
    {
        auto regId = reg.registrationId;
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

    ErrorOrDone yield(Result&& result)
    {
        auto reqId = result.requestId({});
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        bool erased = !result.isProgress({}) || moot;
        if (erased)
            invocations_.erase(found);
        if (moot)
            return false;

        result.setKindToYield({});
        auto done = peer_.send(std::move(result));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            if (!erased)
                invocations_.erase(found);
            peer_.send(Error{{}, MessageKind::invocation, reqId,
                             WampErrc::payloadSizeExceeded});
        }
        return done;
    }

    ErrorOrDone yield(CalleeOutputChunk&& chunk)
    {
        auto reqId = chunk.requestId({});
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

        auto done = peer_.send(std::move(chunk));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            if (!erased)
                invocations_.erase(found);
            peer_.send(Error{{}, MessageKind::invocation, reqId,
                             WampErrc::payloadSizeExceeded});
        }
        return done;
    }

    ErrorOrDone yield(Error&& error)
    {
        auto reqId = error.requestId({});
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        invocations_.erase(found);
        if (moot)
            return false;

        error.setRequestKind({}, MessageKind::invocation);
        return peer_.send(std::move(error));
    }

    WampErrc onInvocation(Invocation&& inv)
    {
        auto regId = inv.registrationId();

        {
            auto kv = procedures_.find(regId);
            if (kv != procedures_.end())
            {
                return onProcedureInvocation(inv, kv->second);
            }
        }

        {
            auto kv = streams_.find(regId);
            if (kv != streams_.end())
            {
                return onStreamInvocation(inv, kv->second);
            }
        }

        return WampErrc::noSuchProcedure;
    }

    void onInterrupt(Interruption&& intr)
    {
        auto found = invocations_.find(intr.requestId());
        if (found == invocations_.end())
            return;

        InvocationRecord& rec = found->second;
        if (rec.interrupted)
            return;
        rec.interrupted = true;
        intr.setRegistrationId({}, rec.registrationId);

        bool interruptHandled = false;

        {
            auto kv = procedures_.find(rec.registrationId);
            if (kv != procedures_.end())
                interruptHandled = onProcedureInterruption(intr, kv->second);
        }

        {
            auto kv = streams_.find(rec.registrationId);
            if (kv != streams_.end())
                interruptHandled = postStreamInterruption(intr, rec);
        }

        if (!interruptHandled)
            automaticallyRespondToInterruption(intr, rec);
    }

    const Uri& lookupProcedureUri(RegistrationId regId) const
    {
        static const Uri empty;
        auto found = procedures_.find(regId);
        if (found == procedures_.end())
            return empty;
        return found->second.uri;
    }

    const Uri& lookupStreamUri(RegistrationId regId) const
    {
        static const Uri empty;
        auto found = streams_.find(regId);
        if (found == streams_.end())
            return empty;
        return found->second.uri;
    }

    void abandonAllStreams(std::error_code ec)
    {
        for (auto& kv: invocations_)
        {
            InvocationRecord& rec = kv.second;
            auto ch = rec.channel.lock();
            if (ch)
                ch->abandon(ec);
        }
    }

    void clear()
    {
        procedures_.clear();
        streams_.clear();
        invocations_.clear();
    }

private:
    using InvocationMap = std::map<RequestId, InvocationRecord>;
    using ProcedureMap = std::map<RegistrationId, ProcedureRegistration>;
    using StreamMap = std::map<RegistrationId, StreamRegistration>;

    WampErrc onProcedureInvocation(Invocation& inv,
                                   const ProcedureRegistration& reg)
    {
        // Progressive calls not allowed on procedures not registered
        // as streams.
        if (inv.isProgress({}) || inv.resultsAreProgressive({}))
            return WampErrc::optionNotAllowed;

        auto requestId = inv.requestId();
        auto registrationId = inv.registrationId();
        auto emplaced = invocations_.emplace(requestId,
                                             InvocationRecord{registrationId});

        // Detect attempt to reinvoke same pending call
        if (!emplaced.second)
            return WampErrc::protocolViolation;

        auto& invocationRec = emplaced.first->second;
        invocationRec.closed = true;
        postRpcRequest(reg.callSlot, inv, reg.registrationId);
        return WampErrc::success;
    }

    bool onProcedureInterruption(Interruption& intr,
                                 const ProcedureRegistration& reg)
    {
        if (reg.interruptSlot == nullptr)
            return false;
        postRpcRequest(reg.interruptSlot, intr, reg.registrationId);
        return true;
    }

    template <typename TSlot, typename TInvocationOrInterruption>
    void postRpcRequest(TSlot slot, TInvocationOrInterruption& request,
                        RegistrationId regId)
    {
        struct Posted
        {
            ClientContext callee;
            TSlot slot;
            TInvocationOrInterruption request;
            RegistrationId regId;

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
                    {
                        callee.yieldResult(std::move(outcome).asResult(),
                                           requestId, regId);
                        break;
                    }

                    case Outcome::Type::error:
                    {
                        callee.yieldError(std::move(outcome).asError(),
                                          requestId, regId);
                        break;
                    }

                    default:
                        assert(false && "unexpected Outcome::Type");
                    }
                }
                catch (Error& error)
                {
                    callee.yieldError(std::move(error), requestId, regId);
                }
                catch (const error::BadType& e)
                {
                    // Forward Variant conversion exceptions as ERROR messages.
                    callee.yieldError(Error{e}, requestId, regId);
                }
            }
        };

        auto slotExec = boost::asio::get_associated_executor(slot);
        request.setExecutor({}, slotExec);
        auto callee = request.callee({});
        assert(!callee.expired());
        Posted posted{std::move(callee), std::move(slot), std::move(request),
                      regId};
        boost::asio::post(
            executor_,
            boost::asio::bind_executor(slotExec, std::move(posted)));
    }

    WampErrc onStreamInvocation(Invocation& inv, const StreamRegistration& reg)
    {
        auto requestId = inv.requestId();
        auto registrationId = inv.registrationId();
        auto emplaced = invocations_.emplace(requestId,
                                             InvocationRecord{registrationId});
        auto& invocationRec = emplaced.first->second;
        if (invocationRec.closed)
            return WampErrc::protocolViolation;

        invocationRec.closed = !inv.isProgress({});
        processStreamInvocation(reg, invocationRec, inv);
        return WampErrc::success;
    }

    void processStreamInvocation(
        const StreamRegistration& reg, InvocationRecord& rec, Invocation& inv)
    {
        if (!rec.invoked)
        {
            auto exec = boost::asio::get_associated_executor(reg.streamSlot);
            inv.setExecutor({}, std::move(exec));
            auto channel = std::make_shared<CalleeChannelImpl>(
                std::move(inv), reg.invitationExpected, executor_);
            rec.channel = channel;
            rec.invoked = true;
            CalleeChannel proxy{{}, channel};

            try
            {
                // Execute the slot directly from this strand in order to avoid
                // a race condition between accept and
                // postInvocation/postInterrupt on the CalleeChannel.
                reg.streamSlot(std::move(proxy));
            }
            catch (Error& error)
            {
                channel->fail(std::move(error));
            }
            catch (const error::BadType& e)
            {
                // Forward Variant conversion exceptions as ERROR messages.
                channel->fail(Error(e));
            }
        }
        else
        {
            auto channel = rec.channel.lock();
            if (channel)
                channel->postInvocation(std::move(inv));
        }
    }

    bool postStreamInterruption(Interruption& intr, InvocationRecord& rec)
    {
        auto channel = rec.channel.lock();
        return bool(channel) && channel->postInterrupt(std::move(intr));
    }

    void automaticallyRespondToInterruption(Interruption& intr,
                                            InvocationRecord& rec)
    {
        // Respond immediately when cancel mode is 'kill' and no interrupt
        // slot is provided.
        // Dealer will have already responded in `killnowait` mode.
        // Dealer does not emit an INTERRUPT in `skip` mode.
        if (intr.cancelMode() == CallCancelMode::kill)
        {
            rec.moot = true;
            Error error{intr.reason().value_or(
                errorCodeToUri(WampErrc::cancelled))};
            error.setRequestId({}, intr.requestId());
            error.setRequestKind({}, MessageKind::invocation);
            peer_.send(std::move(error));
        }
    }

    ProcedureMap procedures_;
    StreamMap streams_;
    InvocationMap invocations_;
    AnyIoExecutor executor_;
    Peer& peer_;
};


//------------------------------------------------------------------------------
// Provides the WAMP client implementation.
//------------------------------------------------------------------------------
class Client final : public std::enable_shared_from_this<Client>,
                     public ClientLike, private PeerListener
{
public:
    using Ptr           = std::shared_ptr<Client>;
    using TransportPtr  = Transporting::Ptr;
    using State         = SessionState;
    using EventSlot     = AnyReusableHandler<void (Event)>;
    using CallSlot      = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;
    using StreamSlot    = AnyReusableHandler<void (CalleeChannel)>;
    using IncidentSlot  = AnyReusableHandler<void (Incident)>;
    using ChallengeSlot = AnyReusableHandler<void (Challenge)>;
    using ChunkSlot     = AnyReusableHandler<void (CallerChannel,
                                                   ErrorOr<CallerInputChunk>)>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(Peer::Ptr peer, AnyIoExecutor exec)
    {
        return Ptr(new Client(std::move(peer), exec, exec));
    }

    static Ptr create(Peer::Ptr peer, AnyIoExecutor exec,
                      AnyCompletionExecutor userExec)
    {
        return Ptr(new Client(std::move(peer), std::move(exec),
                              std::move(userExec)));
    }

    State state() const {return peer_->state();}

    const AnyIoExecutor& executor() const {return executor_;}

    const AnyCompletionExecutor& userExecutor() const {return userExecutor_;}

    const IoStrand& strand() const {return strand_;}

    void observeIncidents(IncidentSlot handler)
    {
        struct Dispatched
        {
            Ptr self;
            IncidentSlot f;
            void operator()() {self->incidentSlot_ = std::move(f);}
        };

        safelyDispatch<Dispatched>(std::move(handler));
    }

    void enableTracing(bool enabled) {PeerListener::enableTracing(enabled);}

    void connect(ConnectionWishList&& w, CompletionHandler<size_t>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            ConnectionWishList w;
            CompletionHandler<size_t> f;
            void operator()() {self->doConnect(std::move(w), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(w), std::move(f));
    }

    void directConnect(any link)
    {
        assert(state() == State::disconnected);
        peer_->connect(strand_, std::move(link));
    }

    void join(Petition&& p, ChallengeSlot c, CompletionHandler<Welcome>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Petition p;
            ChallengeSlot c;
            CompletionHandler<Welcome> f;

            void operator()()
            {
                self->doJoin(std::move(p), std::move(c), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(p), std::move(c), std::move(f));
    }

    void authenticate(Authentication&& a) override
    {
        struct Dispatched
        {
            Ptr self;
            Authentication a;
            void operator()() {self->doAuthenticate(std::move(a));}
        };

        safelyDispatch<Dispatched>(std::move(a));
    }

    void failAuthentication(Reason&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            void operator()() {self->doFailAuthentication(std::move(r));}
        };

        safelyDispatch<Dispatched>(std::move(r));
    }

    void leave(Reason&& r, CompletionHandler<Reason>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Reason r;
            CompletionHandler<Reason> f;
            void operator()() {self->doLeave(std::move(r), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(r), std::move(f));
    }

    void disconnect()
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->doDisconnect();}
        };

        safelyDispatch<Dispatched>();
    }

    void terminate()
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->doTerminate();}
        };

        safelyDispatch<Dispatched>();
    }

    void subscribe(Topic&& t, EventSlot&& s,
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
                self->doSubscribe(std::move(t), std::move(s), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(t), std::move(s), std::move(f));
    }

    void unsubscribe(Subscription s, CompletionHandler<bool>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Subscription s;
            CompletionHandler<bool> f;
            void operator()() {self->doUnsubscribe(s, std::move(f));}
        };

        s.disarm({});
        safelyDispatch<Dispatched>(std::move(s), std::move(f));
    }

    void publish(Pub&& p)
    {
        struct Dispatched
        {
            Ptr self;
            Pub p;
            void operator()() {self->doPublish(std::move(p));}
        };

        safelyDispatch<Dispatched>(std::move(p));
    }

    void publish(Pub&& p, CompletionHandler<PublicationId>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Pub p;
            CompletionHandler<PublicationId> f;
            void operator()() {self->doPublish(std::move(p), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(p), std::move(f));
    }

    void enroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
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
                self->doEnroll(std::move(p), std::move(c), std::move(i),
                             std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(p), std::move(c), std::move(i),
                                   std::move(f));
    }

    void enroll(Stream&& s, StreamSlot&& ss,
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
                self->doEnroll(std::move(s), std::move(ss), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(ss), std::move(f));
    }

    void unregister(const Registration& r) override
    {
        struct Dispatched
        {
            Ptr self;
            Registration r;
            void operator()() {self->doUnregister(r);}
        };

        safelyDispatch<Dispatched>(r);
    }

    void unregister(const Registration& r, CompletionHandler<bool>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Registration r;
            CompletionHandler<bool> f;
            void operator()() {self->doUnregister(std::move(r), std::move(f));}
        };

        safelyDispatch<Dispatched>(r, std::move(f));
    }

    void call(Rpc&& r, CompletionHandler<Result>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Rpc r;
            CompletionHandler<Result> f;
            void operator()() {self->doCall(std::move(r), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(r), std::move(f));
    }

    void requestStream(StreamRequest&& s, ChunkSlot&& c,
                       CompletionHandler<CallerChannel>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            StreamRequest s;
            ChunkSlot c;
            CompletionHandler<CallerChannel> f;

            void operator()()
            {
                self->doRequestStream(std::move(s), std::move(c), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(s), std::move(c), std::move(f));
    }

    void openStream(StreamRequest&& r, ChunkSlot&& c,
                    CompletionHandler<CallerChannel>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            StreamRequest r;
            ChunkSlot c;
            CompletionHandler<CallerChannel> f;

            void operator()()
            {
                self->doOpenStream(std::move(r), std::move(c), std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(r), std::move(c), std::move(f));
    }

private:
    using RequestKey          = typename Message::RequestKey;
    using RequestHandler      = AnyCompletionHandler<void (ErrorOr<Message>)>;

    Client(Peer::Ptr peer, AnyIoExecutor exec, AnyCompletionExecutor userExec)
        : executor_(std::move(exec)),
          userExecutor_(std::move(userExec)),
          strand_(boost::asio::make_strand(executor_)),
          peer_(std::move(peer)),
          readership_(executor_),
          registry_(*peer_, executor_),
          requestor_(*peer_, strand_, executor_, userExecutor_)
    {
        peer_->listen(this);
    }

    void removeSlot(EventSlotTag, EventSlotKey key) override
    {
        struct Dispatched
        {
            Ptr self;
            EventSlotKey key;
            void operator()() {self->doRemoveSlot(key);}
        };

        safelyDispatch<Dispatched>(key);
    }

    void removeSlot(CallSlotTag, SlotId slotId) override
    {
        // TODO
    }

    void onPeerDisconnect() override
    {
        report({IncidentKind::transportDropped});
    }

    void onPeerFailure(std::error_code ec, bool, std::string why) override
    {
        report({IncidentKind::commFailure, ec, std::move(why)});
        abandonPending(ec);
    }

    void onPeerTrace(std::string&& messageDump) override
    {
        if (incidentSlot_ && traceEnabled())
            report({IncidentKind::trace, std::move(messageDump)});
    }

    void onPeerHello(Petition&&) override {assert(false);}

    void onPeerAbort(Reason&& reason, bool wasJoining) override
    {
        if (wasJoining)
            return onWampReply(reason.message({}));

        if (incidentSlot_)
            report({IncidentKind::abortedByPeer, reason});

        abandonPending(reason.errorCode());
    }

    void onPeerChallenge(Challenge&& challenge) override
    {
        if (challengeSlot_)
        {
            challenge.setChallengee({}, makeContext());
            dispatchChallenge(std::move(challenge));
        }
        else
        {
            auto r = Reason{WampErrc::authenticationFailed}
                         .withHint("No challenge handler");
            doFailAuthentication(std::move(r));
        }
    }

    void onPeerAuthenticate(Authentication&& authentication) override
    {
        assert(false);
    }

    void dispatchChallenge(Challenge&& challenge)
    {
        struct Dispatched
        {
            Ptr self;
            Challenge challenge;
            ChallengeSlot slot;

            void operator()()
            {
                try
                {
                    slot(std::move(challenge));
                }
                catch (Reason r)
                {
                    self->failAuthentication(std::move(r));
                }
                catch (const error::BadType& e)
                {
                    self->failAuthentication(Reason{e});
                }
            }
        };

        auto boundExec = boost::asio::get_associated_executor(challengeSlot_);
        Dispatched dispatched{shared_from_this(), std::move(challenge),
                              challengeSlot_};
        boost::asio::dispatch(
            executor_,
            boost::asio::bind_executor(boundExec, std::move(dispatched)));
    }

    void onPeerGoodbye(Reason&& reason, bool wasShuttingDown) override
    {
        if (wasShuttingDown)
        {
            onWampReply(reason.message({}));
            abandonPending(MiscErrc::abandoned);
        }
        else if (incidentSlot_)
        {
            report({IncidentKind::closedByPeer, reason});
            abandonPending(reason.errorCode());
            peer_->close();
        }
    }

    void onPeerMessage(Message&& msg) override
    {
        switch (msg.kind())
        {
        case MessageKind::event:      return onEvent(msg);
        case MessageKind::invocation: return onInvocation(msg);
        case MessageKind::interrupt:  return onInterrupt(msg);
        default:                      return onWampReply(msg);
        }
    }

    void onEvent(Message& msg)
    {
        Event event{{}, std::move(msg)};
        bool ok = readership_.onEvent(event, makeContext());
        if (!ok && incidentSlot_)
        {
            std::ostringstream oss;
            oss << "With subId=" << event.subscriptionId()
                << " and pubId=" << event.publicationId();
            report({IncidentKind::eventError, oss.str()});
        }
    }

    void onInvocation(Message& msg)
    {
        // TODO: Callee-initiated timeouts

        Invocation inv{{}, std::move(msg)};
        inv.setCallee({}, makeContext());
        auto reqId = inv.requestId();
        auto regId = inv.registrationId();

        // Crossbar uses the same INVOCATION request ID generator for
        // all callee sessions.
        // https://github.com/crossbario/crossbar/issues/2081
#ifdef CPPWAMP_STRICT_INVOCATION_ID_CHECKS
        auto maxRequestId = inboundRequestIdWatermark_ + 1u;
        if (reqId > maxRequestId)
        {
            return failProtocol("Router used non-sequential request ID "
                                "in INVOCATION message");
        }
        if (reqId == maxRequestId)
            ++inboundRequestIdWatermark_;
#endif

        switch (registry_.onInvocation(std::move(inv)))
        {
        case WampErrc::success:
            break;

        case WampErrc::noSuchProcedure:
            return onInvocationProcedureNotFound(reqId, regId);

        case WampErrc::optionNotAllowed:
            return onInvocationProgressNotAllowed(reqId, regId);

        case WampErrc::protocolViolation:
            return failProtocol("Router attempted to reinvoke an RPC that is "
                                "closed to further progress");

        default:
            assert(false && "Unexpected WampErrc enumerator");
            break;
        }
    }

    void onInvocationProcedureNotFound(RequestId reqId, RegistrationId regId)
    {
        auto ec = make_error_code(WampErrc::noSuchProcedure);
        report({IncidentKind::trouble, ec,
                "With registration ID " + std::to_string(regId)});
        peer_->send(Error{{}, MessageKind::invocation, reqId, ec});
    }

    void onInvocationProgressNotAllowed(RequestId reqId, RegistrationId regId)
    {
        std::string why{"Router requested progress on an RPC endpoint not "
                        "registered as a stream"};
        auto ec = make_error_code(WampErrc::optionNotAllowed);
        report({IncidentKind::trouble, ec,
                why + ", with registration ID " + std::to_string(regId)});
        Error error({}, MessageKind::invocation, reqId, ec);
        error.withArgs(std::move(why));
        peer_->send(std::move(error));
    }

    void onInterrupt(Message& msg)
    {
        Interruption intr{{}, std::move(msg)};
        intr.setCallee({}, makeContext());
        registry_.onInterrupt(std::move(intr));
    }

    void onWampReply(Message& msg)
    {
        const char* msgName = msg.name();
        assert(msg.isReply());
        if (!requestor_.onReply(std::move(msg)))
        {
            // Ignore spurious RESULT and ERROR responses that can occur
            // due to race conditions.
            using K = MessageKind;
            if ((msg.kind() != K::result) && (msg.kind() != K::error))
            {
                failProtocol(std::string("Received ") + msgName +
                             " response with no matching request");
            }
        }
    }

    void onWelcome(CompletionHandler<Welcome>&& handler, Message& reply,
                   Uri&& realm)
    {
        Welcome info{{}, std::move(reply)};
        info.setRealm({}, std::move(realm));
        completeNow(handler, std::move(info));
    }

    void onJoinAborted(CompletionHandler<Welcome>&& handler, Message& reply,
                       Reason* reasonPtr)
    {
        Reason reason{{}, std::move(reply)};
        const auto& uri = reason.uri();
        WampErrc errc = errorUriToCode(uri);

        if (reasonPtr != nullptr)
            *reasonPtr = std::move(reason);

        completeNow(handler, makeUnexpectedError(errc));
    }

    void doConnect(ConnectionWishList&& wishes,
                   CompletionHandler<size_t>&& handler)
    {
        assert(!wishes.empty());

        if (!peer_->startConnecting())
            return postErrorToHandler(MiscErrc::invalidState, handler);
        isTerminating_ = false;
        currentConnector_ = nullptr;

        // This makes it easier to transport the move-only completion handler
        // through the gauntlet of intermediary handler functions.
        auto sharedHandler =
            std::make_shared<CompletionHandler<size_t>>(std::move(handler));

        establishConnection(std::move(wishes), 0, std::move(sharedHandler));
    }

    void establishConnection(ConnectionWishList&& wishes, size_t index,
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
                    me.peer_->connect(std::move(*transport), std::move(codec));
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
                establishConnection(std::move(wishes), newIndex,
                                    std::move(handler));
            }
            else
            {
                if (wishes.size() > 1)
                    ec = make_error_code(TransportErrc::exhausted);
                peer_->failConnecting(ec);
                completeNow(*handler, UnexpectedError(ec));
            }
        }
    }

    void doJoin(Petition&& realm, ChallengeSlot onChallenge,
                CompletionHandler<Welcome>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<Welcome> handler;
            Uri realm;
            Reason* abortPtr;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                me.challengeSlot_ = nullptr;
                if (me.checkError(reply, handler))
                {
                    if (reply->kind() == MessageKind::welcome)
                    {
                        me.onWelcome(std::move(handler), *reply,
                                     std::move(realm));
                    }
                    else
                    {
                        assert(reply->kind() == MessageKind::abort);
                        me.onJoinAborted(std::move(handler), *reply, abortPtr);
                    }
                }
            }
        };

        if (!peer_->establishSession())
            return postErrorToHandler(MiscErrc::invalidState, handler);

        realm.withOption("agent", Version::agentString())
            .withOption("roles", ClientFeatures::providedRoles());
        challengeSlot_ = std::move(onChallenge);
        Requested requested{shared_from_this(), std::move(handler), realm.uri(),
                            realm.abortReason({})};
        request(std::move(realm), std::move(requested));
    }

    void doAuthenticate(Authentication&& auth)
    {
        if (state() != State::authenticating)
            return;
        auto done = peer_->send(std::move(auth));
        if (!done && incidentSlot_)
            report({IncidentKind::trouble, done.error(),
                    "While sending AUTHENTICATE message"});
    }

    void doFailAuthentication(Reason&& r)
    {
        if (state() != State::authenticating)
            return;

        if (incidentSlot_)
            report({IncidentKind::challengeFailure, r});

        abandonPending(r.errorCode());
        auto done = peer_->abort(std::move(r));
        auto unex = makeUnexpectedError(WampErrc::payloadSizeExceeded);
        if (incidentSlot_ && (done == unex))
        {
            report({IncidentKind::trouble, unex.value(),
                    "While sending ABORT due to authentication failure"});
        }
    }

    void doLeave(Reason&& reason, CompletionHandler<Reason>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<Reason> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkError(reply, handler))
                {
                    me.clear();
                    me.peer_->close();
                    me.completeNow(handler, Reason({}, std::move(*reply)));
                }
            }
        };

        if (!peer_->startShuttingDown())
            return postErrorToHandler(MiscErrc::invalidState, handler);

        if (reason.uri().empty())
            reason.setUri({}, errorCodeToUri(WampErrc::closeRealm));

        request(std::move(reason),
                Requested{shared_from_this(), std::move(handler)});
    }

    void doDisconnect()
    {
        if (state() == State::connecting)
            currentConnector_->cancel();
        clear();
        peer_->disconnect();
    }

    void doTerminate()
    {
        isTerminating_ = true;
        doDisconnect();
    }

    void doSubscribe(Topic&& topic, EventSlot&& slot,
                     CompletionHandler<Subscription>&& handler)
    {
        struct Requested
        {
            Ptr self;
            MatchUri matchUri;
            EventSlot slot;
            CompletionHandler<Subscription> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (!me.checkReply(reply, MessageKind::subscribed, handler))
                    return;
                Subscribed ack{std::move(*reply)};
                auto sub = me.readership_.onSubscribed(
                    ack.subscriptionId(), std::move(matchUri), std::move(slot),
                    me.makeContext());
                me.completeNow(handler, std::move(sub));
            }
        };

        if (!checkState(State::established, handler))
            return;

        MatchUri matchUri{topic};
        auto subscription = readership_.subscribe(matchUri, slot,
                                                  makeContext());
        if (subscription)
            return complete(handler, std::move(subscription));

        Requested requested{shared_from_this(), std::move(matchUri),
                            std::move(slot), std::move(handler)};
        request(std::move(topic), std::move(requested));
    }

    void doRemoveSlot(EventSlotKey key)
    {
        if (readership_.unsubscribe(key))
            sendUnsubscribe(key.first);
    }

    void doUnsubscribe(const Subscription& sub,
                       CompletionHandler<bool>&& handler)
    {
        if (!sub || !readership_.unsubscribe(sub.key({})))
            return complete(handler, false);
        sendUnsubscribe(sub.id(), std::move(handler));
    }

    void onEventError(Error&& error, SubscriptionId subId) override
    {
        struct Dispatched
        {
            Ptr self;
            Error e;
            SubscriptionId s;
            void operator()() {self->reportEventError(e, s);}
        };

        // This can be called from a foreign thread, so we must dispatch
        // to avoid race when accessing incidentSlot_ member.
        safelyDispatch<Dispatched>(std::move(error), subId);
    }

    void reportEventError(Error& error, SubscriptionId subId)
    {
        const auto& uri = readership_.lookupTopicUri(subId);
        if (!uri.empty())
            error["uri"] = uri;
        report({IncidentKind::eventError, error});
    }

    void doPublish(Pub&& pub)
    {
        if (state() != State::established)
            return;
        auto uri = pub.uri();
        auto reqId = requestor_.request(std::move(pub), nullptr);
        if (incidentSlot_ && !reqId)
        {
            report({IncidentKind::trouble, reqId.error(),
                    "While sending unacknowledged PUBLISH message with "
                    "URI '" + uri + "'"});
        }
    }

    void doPublish(Pub&& pub, CompletionHandler<PublicationId>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<PublicationId> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, MessageKind::published, handler))
                {
                    Published ack{std::move(*reply)};
                    me.completeNow(handler, ack.publicationId());
                }
            }
        };

        if (!checkState(State::established, handler))
            return;

        pub.withOption("acknowledge", true);
        request(std::move(pub),
                Requested{shared_from_this(), std::move(handler)});
    }

    void doEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
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
                if (!me.checkReply(reply, MessageKind::registered, f))
                    return;
                Registered ack{std::move(*reply)};
                r.registrationId = ack.registrationId();
                auto reg = me.registry_.enroll(std::move(r), me.makeContext());
                me.completeNow(f, std::move(reg));
            }
        };

        if (!checkState(State::established, f))
            return;

        ProcedureRegistration reg{std::move(c), std::move(i), p.uri(), 0};
        request(std::move(p),
                Requested{shared_from_this(), std::move(reg), std::move(f)});
    }

    void doEnroll(Stream&& s, StreamSlot&& ss,
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
                if (!me.checkReply(reply, MessageKind::registered, f))
                    return;
                Registered ack{std::move(*reply)};
                r.registrationId = ack.registrationId();
                auto reg = me.registry_.enroll(std::move(r), me.makeContext());
                me.completeNow(f, std::move(reg));
            }
        };

        if (!checkState(State::established, f))
            return;

        StreamRegistration reg{std::move(ss), s.uri(), 0,
                               s.invitationExpected()};
        request(std::move(s),
                Requested{shared_from_this(), std::move(reg), std::move(f)});
    }

    void doUnregister(const Registration& reg)
    {
        struct Requested
        {
            void operator()(ErrorOr<Message>)
            {
                // Don't propagate WAMP errors, as we prefer this
                // to be a no-fail cleanup operation.
            }
        };

        if (registry_.unregister(reg) && state() == State::established)
        {
            Unregister unreg{reg.id()};
            request(unreg, Requested{});
        }
    }

    void doUnregister(const Registration& reg,
                      CompletionHandler<bool>&& handler)
    {
        struct Requested
        {
            Ptr self;
            CompletionHandler<bool> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, MessageKind::unregistered, handler))
                    me.completeNow(handler, true);
            }
        };

        if (registry_.unregister(reg))
        {
            Unregister cmd(reg.id());
            if (checkState(State::established, handler))
                request(Unregister{reg.id()},
                        Requested{shared_from_this(), std::move(handler)});
        }
        else
        {
            complete(handler, false);
        }
    }

    void doCall(Rpc&& rpc, CompletionHandler<Result>&& handler)
    {
        struct Requested
        {
            Ptr self;
            Error* errorPtr;
            CompletionHandler<Result> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, MessageKind::result, handler,
                                  errorPtr))
                {
                    me.completeNow(handler, Result({}, std::move(*reply)));
                }
            }
        };

        if (!checkState(State::established, handler))
            return;

        auto boundCancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = requestor_.request(
            std::move(rpc),
            rpc.callerTimeout(),
            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
        if (!requestId)
            return;

        auto& rpcCancelSlot = rpc.cancellationSlot({});
        if (rpcCancelSlot.is_connected())
        {
            rpcCancelSlot.emplace(makeContext(), *requestId);
        }
        else if (boundCancelSlot.is_connected())
        {
            auto self = shared_from_this();
            auto reqId = *requestId;
            auto mode = rpc.cancelMode();
            boundCancelSlot.assign(
                [this, self, reqId, mode](boost::asio::cancellation_type_t)
                {
                    cancelCall(reqId, mode);
                });
        }
    }

    void cancelCall(RequestId r, CallCancelMode m) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            CallCancelMode m;
            void operator()() {self->doCancelCall(r, m);}
        };

        safelyDispatch<Dispatched>(r, m);
    }

    void doCancelCall(RequestId reqId, CallCancelMode mode)
    {
        if (state() == State::established)
            requestor_.cancelCall(reqId, mode);
    }

    void doRequestStream(StreamRequest&& req, ChunkSlot&& onChunk,
                         CompletionHandler<CallerChannel>&& handler)
    {
        if (!checkState(State::established, handler))
            return;

        requestor_.requestStream(true, makeContext(), std::move(req),
                                 std::move(onChunk), std::move(handler));
    }

    void doOpenStream(StreamRequest&& req, ChunkSlot&& onChunk,
                      CompletionHandler<CallerChannel>&& handler)
    {
        if (!checkState(State::established, handler))
            return;

        auto channel = requestor_.requestStream(
            false, makeContext(), std::move(req), std::move(onChunk));
        complete(handler, std::move(channel));
    }

    ErrorOrDone sendCallerChunk(CallerOutputChunk&& c) override
    {
        struct Dispatched
        {
            Ptr self;
            CallerOutputChunk c;
            void operator()() {self->doSendCallerChunk(std::move(c));}
        };

        if (state() != State::established)
            return makeUnexpectedError(MiscErrc::invalidState);
        safelyDispatch<Dispatched>(std::move(c));
        return true;
    }

    void doSendCallerChunk(CallerOutputChunk&& chunk)
    {
        if (state() != State::established)
            return;

        auto done = requestor_.sendCallerChunk(std::move(chunk));
        if (incidentSlot_ && !done)
        {
            report({IncidentKind::trouble, done.error(),
                    "While sending streaming CALL message"});
        }
    }

    void cancelStream(RequestId r) override
    {
        // As per the WAMP spec, a router supporting progressive
        // calls/invocations must also support call cancellation.
        cancelCall(r, CallCancelMode::killNoWait);
    }

    ErrorOrDone yieldChunk(CalleeOutputChunk&& c, RequestId reqId,
                           RegistrationId regId) override
    {
        struct Dispatched
        {
            Ptr self;
            CalleeOutputChunk c;
            RegistrationId r;
            void operator()() {self->doYieldChunk(std::move(c), r);}
        };

        if (state() != State::established)
            return makeUnexpectedError(MiscErrc::invalidState);
        c.setRequestId({}, reqId);
        safelyDispatch<Dispatched>(std::move(c), regId);
        return true;
    }

    void doYieldChunk(CalleeOutputChunk&& chunk, RegistrationId regId)
    {
        if (state() != State::established)
            return;
        auto done = registry_.yield(std::move(chunk));
        if (incidentSlot_ && !done)
        {
            std::ostringstream oss;
            oss << "Stream RESULT with requestId=" << chunk.requestId({})
                << ", for registrationId=" << regId;
            const auto& uri = registry_.lookupStreamUri(regId);
            if (!uri.empty())
                oss << " and uri=" << uri;
            report({IncidentKind::trouble, done.error(), oss.str()});
        }
    }

    void yieldResult(Result&& r, RequestId reqId, RegistrationId regId) override
    {
        struct Dispatched
        {
            Ptr self;
            Result r;
            RegistrationId i;
            void operator()() {self->doYieldResult(std::move(r), i);}
        };

        r.setRequestId({}, reqId);
        safelyDispatch<Dispatched>(std::move(r), regId);
    }

    void doYieldResult(Result&& result, RegistrationId regId)
    {
        if (state() != State::established)
            return;
        auto done = registry_.yield(std::move(result));
        if (incidentSlot_ && !done)
        {
            std::ostringstream oss;
            oss << "RPC RESULT with requestId=" << result.requestId({})
                << ", for registrationId=" << regId;
            const auto& uri = registry_.lookupProcedureUri(regId);
            if (!uri.empty())
                oss << " and uri=" << uri;
            report({IncidentKind::trouble, done.error(), oss.str()});
        }
    }

    void yieldError(Error&& e, RequestId reqId, RegistrationId regId) override
    {
        struct Dispatched
        {
            Ptr self;
            Error e;
            RegistrationId r;
            void operator()() {self->doYieldError(std::move(e), r);}
        };

        e.setRequestId({}, reqId);
        safelyDispatch<Dispatched>(std::move(e), regId);
    }

    void doYieldError(Error&& error, RegistrationId regId)
    {
        if (state() != State::established)
            return;
        auto done = registry_.yield(std::move(error));
        if (incidentSlot_ && !done)
        {
            std::ostringstream oss;
            oss << "INVOCATION ERROR with requestId=" << error.requestId({})
                << ", for registrationId=" << regId;
            auto uri = registry_.lookupProcedureUri(regId);
            if (!uri.empty())
                oss << " and uri=" << uri;
            report({IncidentKind::trouble, done.error(), oss.str()});
        }
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        F dispatched{shared_from_this(), std::forward<Ts>(args)...};
        boost::asio::dispatch(strand_, std::move(dispatched));
    }

    template <typename F>
    bool checkState(State expectedState, F& handler)
    {
        bool valid = state() == expectedState;
        if (!valid)
            postErrorToHandler(MiscErrc::invalidState, handler);
        return valid;
    }

    template <typename TErrc, typename THandler>
    void postErrorToHandler(TErrc errc, THandler& f)
    {
        auto unex = makeUnexpectedError(errc);
        if (!isTerminating_)
            postAny(executor_, std::move(f), std::move(unex));
    }

    template <typename TInfo>
    ErrorOr<RequestId> request(TInfo&& info, RequestHandler&& handler)
    {
        return requestor_.request(std::move(info), std::move(handler));
    }

    void abandonPending(std::error_code ec)
    {
        if (isTerminating_)
        {
            requestor_.clear();
        }
        else
        {
            requestor_.abandonAll(ec);
            registry_.abandonAllStreams(ec);
        }
    }

    template <typename TErrc>
    void abandonPending(TErrc errc) {abandonPending(make_error_code(errc));}

    void clear()
    {
        abandonPending(MiscErrc::abandoned);
        readership_.clear();
        registry_.clear();

#ifdef CPPWAMP_STRICT_INVOCATION_ID_CHECKS
        inboundRequestIdWatermark_ = 0;
#endif
    }

    void sendUnsubscribe(SubscriptionId subId)
    {
        struct Requested
        {
            void operator()(ErrorOr<Message>)
            {
                // Don't propagate WAMP errors, as we prefer
                // this to be a no-fail cleanup operation.
            }
        };

        if (state() == State::established)
            request(Unsubscribe{subId}, Requested{});
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
                if (me.checkReply(reply, MessageKind::unsubscribed, handler))
                {
                    me.completeNow(handler, true);
                }
            }
        };

        if (checkState(State::established, handler))
        {
            request(Unsubscribe{subId},
                    Requested{shared_from_this(), std::move(handler)});
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
    bool checkReply(ErrorOr<Message>& reply, MessageKind kind,
                    THandler& handler, Error* errorPtr = nullptr)
    {
        if (!checkError(reply, handler))
            return false;

        if (reply->kind() != MessageKind::error)
        {
            assert((reply->kind() == kind) &&
                   "Unexpected WAMP message type");
            return true;
        }

        Error error({}, std::move(*reply));
        WampErrc errc = error.errorCode();

        if (errorPtr != nullptr)
            *errorPtr = std::move(error);

        dispatchHandler(handler, makeUnexpectedError(errc));
        return false;
    }

    void report(Incident&& incident)
    {
        if (incidentSlot_)
            dispatchHandler(incidentSlot_, std::move(incident));
    }

    void failProtocol(std::string why)
    {
        peer_->abort(Reason(WampErrc::protocolViolation).withHint(why));
        auto ec = make_error_code(WampErrc::protocolViolation);
        report({IncidentKind::commFailure, ec, std::move(why)});
    }

    template <typename S, typename... Ts>
    void dispatchHandler(AnyCompletionHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        dispatchAny(executor_, std::move(f), std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void dispatchHandler(const AnyReusableHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        dispatchAny(executor_, f, std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void postHandler(const AnyReusableHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        postAny(executor_, f, std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void complete(AnyCompletionHandler<S>& f, Ts&&... args)
    {
        if (isTerminating_)
            return;
        postAny(executor_, std::move(f), std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void completeNow(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        dispatchHandler(handler, std::forward<Ts>(args)...);
    }

    ClientContext makeContext() {return ClientContext{shared_from_this()};}

    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    IoStrand strand_;
    Peer::Ptr peer_;
    Readership readership_;
    ProcedureRegistry registry_;
    Requestor requestor_;
    IncidentSlot incidentSlot_;
    ChallengeSlot challengeSlot_;
    Connecting::Ptr currentConnector_;
#ifdef CPPWAMP_STRICT_INVOCATION_ID_CHECKS
    RequestId inboundRequestIdWatermark_ = 0;
#endif
    bool isTerminating_ = false;

    friend class ClientContext;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
