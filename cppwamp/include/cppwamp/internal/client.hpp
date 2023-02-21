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
#include "../codec.hpp"
#include "../chits.hpp"
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
#include "subscriber.hpp"
#include "peer.hpp"
#include "timeoutscheduler.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class Requestor
{
public:
    using Message          = WampMessage;
    using OneShotHandler   = AnyCompletionHandler<void (ErrorOr<Message>)>;
    using MultiShotHandler = std::function<void (ErrorOr<Message>)>;

    Requestor(IoStrand strand) : strand_(std::move(strand)) {}

    ErrorOr<RequestId> request(Peer& peer, Message& msg,
                               OneShotHandler&& handler)
    {
        return sendRequest(peer, msg, oneShotRequestMap_, std::move(handler));
    }

    ErrorOr<RequestId> ongoingRequest(Peer& peer, Message& msg,
                                      MultiShotHandler&& handler)
    {
        return sendRequest(peer, msg, multiShotRequestMap_, std::move(handler));
    }

    bool onReply(Message& msg)
    {
        assert(msg.isReply());
        bool matchingRequestFound = false;
        auto key = msg.requestKey();
        auto kv = oneShotRequestMap_.find(key);
        if (kv != oneShotRequestMap_.end())
        {
            matchingRequestFound = true;
            auto handler = std::move(kv->second);
            oneShotRequestMap_.erase(kv);
            handler(std::move(msg));
        }
        else
        {
            auto kv = multiShotRequestMap_.find(key);
            if (kv != multiShotRequestMap_.end())
            {
                matchingRequestFound = true;
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

        return matchingRequestFound;
    }

    bool cancelCall(const CallCancellation& cancellation)
    {
        // If the cancel mode is not 'kill', don't wait for the router's
        // ERROR message and post the request handler immediately
        // with a WampErrc::cancelled error code.

        bool found = false;
        RequestKey key{WampMsgType::call, cancellation.requestId()};
        auto unex = makeUnexpectedError(WampErrc::cancelled);

        auto kv = oneShotRequestMap_.find(key);
        if (kv != oneShotRequestMap_.end())
        {
            found = true;
            if (cancellation.mode() != CallCancelMode::kill)
            {
                auto handler = std::move(kv->second);
                oneShotRequestMap_.erase(kv);
                completeRequest(handler, unex);
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
                    completeRequest(handler, unex);
                }
            }
        }

        return found;
    }

    void abortAll(std::error_code ec)
    {
        UnexpectedError unex{ec};
        for (auto& kv: oneShotRequestMap_)
            completeRequest(kv.second, unex);
        for (auto& kv: multiShotRequestMap_)
            completeRequest(kv.second, unex);
        clear();
    }

    void clear()
    {
        oneShotRequestMap_.clear();
        multiShotRequestMap_.clear();
    }

private:
    template <typename TRequestMap, typename THandler>
    ErrorOr<RequestId> sendRequest(Peer& peer, Message& msg,
                                   TRequestMap& requests, THandler&& handler)
    {
        assert(msg.type() != WampMsgType::none);
        RequestId requestId = nullId();
        if (msg.isRequest())
        {
            requestId = nextRequestId_ + 1;
            // Will take 285 years to overflow 2^53 at 1 million requests/sec
            assert(nextRequestId_ <= 9007199254740992u);
            msg.setRequestId(requestId);
        }

        auto sent = peer.send(msg);
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        if (msg.isRequest())
            ++nextRequestId_;

        auto emplaced = requests.emplace(msg.requestKey(), std::move(handler));
        assert(emplaced.second);
        return requestId;
    }

    template <typename F, typename... Ts>
    void completeRequest(F& handler, Ts&&... args)
    {
        boost::asio::post(
            strand_,
            std::bind(std::move(handler), std::forward<Ts>(args)...));
    }

    using RequestKey          = typename Message::RequestKey;
    using OneShotRequestMap   = std::map<RequestKey, OneShotHandler>;
    using MultiShotRequestMap = std::map<RequestKey, MultiShotHandler>;

    IoStrand strand_;
    OneShotRequestMap oneShotRequestMap_;
    MultiShotRequestMap multiShotRequestMap_;
    RequestId nextRequestId_ = nullId();
};


//------------------------------------------------------------------------------
class Readership
{
public:
    using SubscriberPtr = std::shared_ptr<Subscriber>;
    using EventSlot = AnyReusableHandler<void (Event)>;

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
    bool onEvent(EventMessage& eventMsg, SubscriberPtr subscriber,
                 AnyIoExecutor& exec, AnyCompletionExecutor& userExec)
    {
        auto found = subscriptions_.find(eventMsg.subscriptionId());
        if (found == subscriptions_.end())
            return false;

        const auto& record = found->second;
        assert(!record.slots.empty());
        Event event({}, userExec, std::move(eventMsg));
        for (const auto& kv: record.slots)
        {
            const auto& slot = kv.second;
            postEvent(slot, record.topic.uri(), event, subscriber, exec,
                      userExec);
        }
        return true;
    }

    void clear()
    {
        byTopic_.clear();
        subscriptions_.clear();
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
    SlotId nextSlotId_ = 0;
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
    using LogHandler         = AnyReusableHandler<void(LogEntry)>;
    using StateChangeHandler = AnyReusableHandler<void(SessionState,
                                                       std::error_code)>;
    using ChallengeHandler   = AnyReusableHandler<void(Challenge)>;
    using OngoingCallHandler = AnyReusableHandler<void(ErrorOr<Result>)>;

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
                {"progressive_call_results", true}
            }}}}},
            {"caller", Object{{"features", Object{{
                {"call_canceling", true},
                {"call_timeout", true},
                {"caller_identification", true},
                {"progressive_call_results", true}
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

    void enroll(Procedure&& procedure, CallSlot&& callSlot,
                InterruptSlot&& interruptSlot,
                CompletionHandler<Registration>&& handler)
    {
        struct Requested
        {
            Ptr self;
            RegistrationRecord rec;
            CompletionHandler<Registration> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, WampMsgType::registered, handler))
                {
                    const auto& msg = messageCast<RegisteredMessage>(*reply);
                    auto regId = msg.registrationId();
                    Registration reg(self, regId, {});
                    me.registry_[regId] = std::move(rec);
                    me.completeNow(handler, std::move(reg));
                }
            }
        };

        if (!checkState(State::established, handler))
            return;

        RegistrationRecord rec{std::move(callSlot), std::move(interruptSlot)};
        request(procedure.message({}),
                Requested{shared_from_this(), std::move(rec),
                          std::move(handler)});
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

        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            if (state() == State::established)
            {
                UnregisterMessage msg(reg.id());
                request(msg, Requested{shared_from_this()});
            }
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
                {
                    me.completeNow(handler, true);
                }
            }
        };

        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
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

    void oneShotCall(Rpc&& rpc, CallChit* chitPtr,
                     CompletionHandler<Result>&& handler)
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

        if (chitPtr)
            *chitPtr = CallChit{};

        if (!checkState(State::established, handler))
            return;

        auto cancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = request(
            rpc.message({}),
            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
        if (!requestId)
            return;

        CallChit chit{shared_from_this(), *requestId, rpc.cancelMode(), {}};

        if (cancelSlot.is_connected())
        {
            cancelSlot.assign(
                [chit](boost::asio::cancellation_type_t) {chit.cancel();});
        }

        if (rpc.callerTimeout().count() != 0)
            timeoutScheduler_->insert(rpc.callerTimeout(), *requestId);

        if (chitPtr)
            *chitPtr = chit;
    }

    void safeOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Rpc r;
            CallChit* c;
            CompletionHandler<Result> f;

            void operator()()
            {
                self->oneShotCall(std::move(r), c, std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(r), c, std::move(f));
    }

    void ongoingCall(Rpc&& rpc, CallChit* chitPtr, OngoingCallHandler&& handler)
    {
        struct Requested
        {
            Ptr self;
            Error* errorPtr;
            OngoingCallHandler handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, WampMsgType::result, handler,
                                  errorPtr))
                {
                    auto& resultMsg = messageCast<ResultMessage>(*reply);
                    me.dispatchHandler(handler,
                                       Result({}, std::move(resultMsg)));
                }
            }
        };

        if (chitPtr)
            *chitPtr = CallChit{};

        if (!checkState(State::established, handler))
            return;

        rpc.withProgressiveResults(true);

        auto cancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = ongoingRequest(
            rpc.message({}),
            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
        if (!requestId)
            return;

        CallChit chit{shared_from_this(), *requestId, rpc.cancelMode(), {}};

        if (cancelSlot.is_connected())
        {
            cancelSlot.assign(
                [chit](boost::asio::cancellation_type_t) {chit.cancel();});
        }

        if (rpc.callerTimeout().count() != 0)
            timeoutScheduler_->insert(rpc.callerTimeout(), *requestId);

        if (chitPtr)
            *chitPtr = chit;
    }

    void safeOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Rpc r;
            CallChit* c;
            OngoingCallHandler f;

            void operator()()
            {
                self->ongoingCall(std::move(r), c, std::move(f));
            }
        };

        safelyDispatch<Dispatched>(std::move(r), c, std::move(f));
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

    ErrorOrDone yield(RequestId reqId, Result&& result) override
    {
        if (state() != State::established)
            return makeUnexpectedError(Errc::invalidState);

        auto found = pendingInvocations_.find(reqId);
        if (found == pendingInvocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool expired = found->second.expired;
        bool erased = !result.isProgressive() || expired;
        if (erased)
            pendingInvocations_.erase(found);
        if (expired)
            return false;

        auto done = peer_.send(result.yieldMessage({}, reqId));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            if (!erased)
                pendingInvocations_.erase(found);
            peer_.sendError(WampMsgType::invocation, reqId,
                            Error{WampErrc::payloadSizeExceeded});
        }
        return done;
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

        auto found = pendingInvocations_.find(reqId);
        if (found == pendingInvocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool expired = found->second.expired;
        pendingInvocations_.erase(found);
        if (expired)
            return false;

        return peer_.sendError(WampMsgType::invocation, reqId,
                               std::move(error));
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

    struct RegistrationRecord
    {
        CallSlot callSlot;
        InterruptSlot interruptSlot;
    };

    struct InvocationRecord
    {
        InvocationRecord(RegistrationId regId) : registrationId(regId) {}

        RegistrationId registrationId;
        bool interrupted = false;
        bool expired = false;
    };

    using SlotId                 = uint64_t;
    using Registry               = std::map<RegistrationId, RegistrationRecord>;
    using InvocationMap          = std::map<RequestId, InvocationRecord>;
    using CallerTimeoutDuration  = typename Rpc::TimeoutDuration;
    using CallerTimeoutScheduler = TimeoutScheduler<RequestId>;
    using Message                = WampMessage;
    using RequestKey             = typename Message::RequestKey;
    using OneShotHandler         = AnyCompletionHandler<void (ErrorOr<Message>)>;
    using MultiShotHandler       = std::function<void (ErrorOr<Message>)>;

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
          requestor_(strand_),
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

    ErrorOr<RequestId> request(Message& msg, OneShotHandler&& handler)
    {
        return requestor_.request(peer_, msg, std::move(handler));
    }

    ErrorOr<RequestId> ongoingRequest(Message& msg, MultiShotHandler&& handler)
    {
        return requestor_.ongoingRequest(peer_, msg, std::move(handler));
    }

    void abortPending(std::error_code ec)
    {
        if (isTerminating_)
            requestor_.clear();
        else
            requestor_.abortAll(ec);
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
        pendingInvocations_.clear();
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
        bool ok = readership_.onEvent(eventMsg, shared_from_this(), executor_,
                                      userExecutor_);
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
        auto requestId = invMsg.requestId();
        auto regId = invMsg.registrationId();

        if (requestId <= lastInvocationRequestId_)
        {
            auto err = Error(WampErrc::protocolViolation)
                           .withArgs("Non-monotonic request ID");
            peer_.sendError(WampMsgType::invocation, requestId, std::move(err));
            log(LogLevel::error,
                "Rejected INVOCATION with non-monotonic request ID "
                    + std::to_string(requestId));
            return;
        }

        lastInvocationRequestId_ = requestId;

        auto kv = registry_.find(regId);
        if (kv == registry_.end())
        {
            peer_.sendError(WampMsgType::invocation, requestId,
                            {WampErrc::noSuchProcedure});
            log(LogLevel::error,
                "No matching procedure for INVOCATION with registration ID "
                    + std::to_string(regId));
            return;
        }

        Invocation inv({}, shared_from_this(), userExecutor_,
                       std::move(invMsg));

        const RegistrationRecord& rec = kv->second;
        auto emplaced = pendingInvocations_.emplace(requestId,
                                                    InvocationRecord{regId});
        assert(emplaced.second);
        postRpcRequest(rec.callSlot, std::move(inv));
    }

    void onInterrupt(Message& msg)
    {
        auto& interruptMsg = messageCast<InterruptMessage>(msg);
        auto found = pendingInvocations_.find(interruptMsg.requestId());
        if (found == pendingInvocations_.end())
            return;

        InvocationRecord& rec = found->second;
        if (rec.interrupted)
            return;
        rec.interrupted = true;

        Interruption intr({}, shared_from_this(), userExecutor_,
                          std::move(interruptMsg));
        auto kv = registry_.find(rec.registrationId);
        if (kv != registry_.end() && kv->second.interruptSlot != nullptr)
        {
            const RegistrationRecord& rec = kv->second;
            postRpcRequest(rec.interruptSlot, std::move(intr));
        }
        else if (intr.cancelMode() == CallCancelMode::kill)
        {
            // Respond immediately when cancel mode is 'kill' and no interrupt
            // slot is provided.
            rec.expired = true;
            Error error{intr.reason().value_or(
                errorCodeToUri(WampErrc::cancelled))};
            peer_.sendError(WampMsgType::invocation, intr.requestId(),
                            std::move(error));
        }
    }

    template <typename TSlot, typename TInvocationOrInterruption>
    void postRpcRequest(TSlot slot, TInvocationOrInterruption&& request)
    {
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
                    Outcome outcome(slot(std::move(request)));
                    switch (outcome.type())
                    {
                    case Outcome::Type::deferred:
                        // Do nothing
                        break;

                    case Outcome::Type::result:
                        me.safeYield(requestId, std::move(outcome).asResult());
                        break;

                    case Outcome::Type::error:
                        me.safeYield(requestId, std::move(outcome).asError());
                        break;

                    default:
                        assert(false && "unexpected Outcome::Type");
                    }
                }
                catch (Error& error)
                {
                    me.yield(requestId, std::move(error));
                }
                catch (const error::BadType& e)
                {
                    // Forward Variant conversion exceptions as ERROR messages.
                    me.yield(requestId, Error(e)).value();
                }
            }
        };

        auto exec = boost::asio::get_associated_executor(slot, userExecutor_);
        Posted posted{shared_from_this(), std::move(slot), std::move(request)};
        boost::asio::post(executor_,
                          boost::asio::bind_executor(exec, std::move(posted)));
    }

    void onWampReply(Message& msg)
    {
        assert(msg.isReply());
        if (!requestor_.onReply(msg))
        {
            log(LogLevel::warning,
                "Discarding received " + std::string(msg.name()) +
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

    SlotId nextSlotId() {return nextSlotId_++;}

    Peer peer_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    IoStrand strand_;
    Connecting::Ptr currentConnector_;
    Readership readership_;
    Registry registry_;
    Requestor requestor_;
    InvocationMap pendingInvocations_;
    CallerTimeoutScheduler::Ptr timeoutScheduler_;
    LogHandler logHandler_;
    StateChangeHandler stateChangeHandler_;
    ChallengeHandler challengeHandler_;
    SlotId nextSlotId_ = 0;
    RequestId lastInvocationRequestId_ = nullId();
    bool isTerminating_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
