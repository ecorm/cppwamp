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
#include "callertimeout.hpp"
#include "challengee.hpp"
#include "subscriber.hpp"
#include "peer.hpp"

namespace wamp
{

namespace internal
{

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

    const AnyIoExecutor& executor() const {return peer_.executor();}

    const IoStrand& strand() const {return peer_.strand();}

    const AnyCompletionExecutor& userExecutor() const
    {
        return peer_.userExecutor();
    }

    void setLogHandler(LogHandler handler)
    {
        peer_.setLogHandler(std::move(handler));
    }

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
        peer_.setStateChangeHandler(std::move(f));
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
                        me.onWelcome(std::move(handler), std::move(*reply),
                                     std::move(realmUri));
                    }
                    else
                    {
                        assert(reply->type() == WampMsgType::abort);
                        me.onJoinAborted(std::move(handler), std::move(*reply),
                                         abortPtr);
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
        peer_.request(realm.message({}),
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
                    auto& goodBye = messageCast<GoodbyeMessage>(*reply);
                    me.completeNow(handler, Reason({}, std::move(goodBye)));
                }
            }
        };

        if (!checkState(State::established, handler))
            return;
        timeoutScheduler_->clear();
        peer_.closeSession(std::move(reason),
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
        if (state() == State::connecting)
            currentConnector_->cancel();
        clear();
        peer_.terminate();
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
            SubscriptionRecord rec;
            CompletionHandler<Subscription> handler;

            void operator()(ErrorOr<Message> reply)
            {
                auto& me = *self;
                if (me.checkReply(reply, WampMsgType::subscribed, handler))
                {
                    const auto& msg = messageCast<SubscribedMessage>(*reply);
                    auto subId = msg.subscriptionId();
                    auto slotId = me.nextSlotId();
                    Subscription sub(self, subId, slotId, {});
                    rec.topicMap->emplace(rec.topicUri, subId);
                    me.readership_[subId][slotId] = std::move(rec);
                    me.completeNow(handler, std::move(sub));
                }
            }
        };

        assert(topic.matchPolicy() != MatchPolicy::unknown);
        if (!checkState(State::established, handler))
            return;

        SubscriptionRecord rec = {topic.uri(), std::move(slot), nullptr};

        switch (topic.matchPolicy())
        {
        case MatchPolicy::exact:    rec.topicMap = &exactTopics_;    break;
        case MatchPolicy::prefix:   rec.topicMap = &prefixTopics_;   break;
        case MatchPolicy::wildcard: rec.topicMap = &wildcardTopics_; break;
        default: assert(false && "Unexpected MatchPolicy enumerator");
        }

        auto kv = rec.topicMap->find(rec.topicUri);
        if (kv == rec.topicMap->end())
        {
            peer_.request(
                topic.message({}),
                Requested{shared_from_this(), std::move(rec),
                          std::move(handler)});
        }
        else
        {
            auto subId = kv->second;
            auto slotId = nextSlotId();
            Subscription sub{shared_from_this(), subId, slotId, {}};
            readership_[subId][slotId] = std::move(rec);
            complete(handler, std::move(sub));
        }
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
        auto kv = readership_.find(sub.id());
        if (kv != readership_.end())
        {
            auto& localSubs = kv->second;
            if (!localSubs.empty())
            {
                auto subKv = localSubs.find(sub.slotId({}));
                if (subKv != localSubs.end())
                {
                    auto& localSub = subKv->second;
                    if (localSubs.size() == 1u)
                        localSub.topicMap->erase(localSub.topicUri);

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
        auto kv = readership_.find(sub.id());
        if (kv != readership_.end())
        {
            auto& localSubs = kv->second;
            if (!localSubs.empty())
            {
                auto subKv = localSubs.find(sub.slotId({}));
                if (subKv != localSubs.end())
                {
                    auto& localSub = subKv->second;
                    if (localSubs.size() == 1u)
                        localSub.topicMap->erase(localSub.topicUri);

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
            complete(handler, false);
        }
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
        peer_.request(pub.message({}),
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
        peer_.request(procedure.message({}),
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
                self->checkReply(reply, WampMsgType::unregistered);
            }
        };

        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            if (state() == State::established)
            {
                UnregisterMessage msg(reg.id());
                peer_.request(msg, Requested{shared_from_this()});
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
            {
                peer_.request(msg, Requested{shared_from_this(),
                                                 std::move(handler)});
            }
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
        auto requestId = peer_.request(
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
        auto requestId = peer_.ongoingRequest(
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
        return peer_.cancelCall(CallCancellation{reqId, mode});
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
    using ErrorOrDonePromise = std::promise<ErrorOrDone>;
    using TopicMap = std::map<std::string, SubscriptionId>;

    struct SubscriptionRecord
    {
        String topicUri;
        EventSlot slot;
        TopicMap* topicMap;
    };

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

    using Message        = WampMessage;
    using SlotId         = uint64_t;
    using LocalSubs      = std::map<SlotId, SubscriptionRecord>;
    using Readership     = std::map<SubscriptionId, LocalSubs>;
    using Registry       = std::map<RegistrationId, RegistrationRecord>;
    using InvocationMap  = std::map<RequestId, InvocationRecord>;
    using CallerTimeoutDuration = typename Rpc::TimeoutDuration;

    Client(const AnyIoExecutor& exec, AnyCompletionExecutor userExec)
        : peer_(false, exec, std::move(userExec)),
          timeoutScheduler_(CallerTimeoutScheduler::create(strand()))
    {
        peer_.setInboundMessageHandler(
            [this](Message msg) {onInbound(std::move(msg));} );
    }

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand(), F{shared_from_this(), std::forward<Ts>(args)...});
    }

    template <typename F>
    bool checkState(State expectedState, F& handler)
    {
        bool valid = state() == expectedState;
        if (!valid)
        {
            auto unex = makeUnexpectedError(Errc::invalidState);
            if (!peer_.isTerminating())
            {
                postVia(executor(), userExecutor(), std::move(handler),
                        std::move(unex));
            }
        }
        return valid;
    }

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
                if (me.peer_.isTerminating())
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

        currentConnector_ = wishes.at(index).makeConnector(strand());
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
        exactTopics_.clear();
        prefixTopics_.clear();
        wildcardTopics_.clear();
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
                self->checkReply(reply, WampMsgType::unsubscribed);
            }
        };

        if (state() == State::established)
        {
            UnsubscribeMessage msg(subId);
            peer_.request(msg, Requested{shared_from_this()});
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
            peer_.request(msg, Requested{shared_from_this(),
                                         std::move(handler)});
        }
    }

    void onInbound(Message msg)
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

    void onWelcome(CompletionHandler<Welcome>&& handler, Message&& reply,
                   String&& realmUri)
    {
        std::weak_ptr<Client> self = shared_from_this();
        timeoutScheduler_->listen([self](RequestId reqId)
        {
            auto ptr = self.lock();
            if (ptr)
                ptr->cancelCall(reqId, CallCancelMode::killNoWait);
        });

        auto& welcomeMsg = messageCast<WelcomeMessage>(reply);
        Welcome info{{}, std::move(realmUri), std::move(welcomeMsg)};
        completeNow(handler, std::move(info));
    }

    void onJoinAborted(CompletionHandler<Welcome>&& handler, Message&& reply,
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

    void onChallenge(Message&& msg)
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
                oss << "Received a CHALLENGE with no registered handler "
                       "(with method=" << challenge.method() << " extra="
                    << challenge.options() << ")";
                log(LogLevel::error, oss.str());
            }

            // Send empty signature to avoid deadlock with other peer.
            authenticate(Authentication(""));
        }
    }

    void onEvent(Message&& msg)
    {
        auto& eventMsg = messageCast<EventMessage>(msg);
        auto kv = readership_.find(eventMsg.subscriptionId());
        if (kv != readership_.end())
        {
            const auto& localSubs = kv->second;
            assert(!localSubs.empty());
            Event event({}, userExecutor(), std::move(eventMsg));
            for (const auto& subKv: localSubs)
                postEvent(subKv.second, event);
        }
        else if (logLevel() <= LogLevel::warning)
        {
            std::ostringstream oss;
            oss << "Received an EVENT that is not subscribed to "
                   "(with subId=" << eventMsg.subscriptionId()
                << " pubId=" << eventMsg.publicationId() << ")";
            log(LogLevel::warning, oss.str());
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
                    me.warnEventError(e, subId, pubId);
                }
                catch (const error::BadType& e)
                {
                    me.warnEventError(Error(e), subId, pubId);
                }
            }
        };

        auto exec = boost::asio::get_associated_executor(sub.slot,
                                                         userExecutor());
        Posted posted{shared_from_this(), sub.slot, event};
        boost::asio::post(executor(),
                          boost::asio::bind_executor(exec, std::move(posted)));
    }

    void warnEventError(const Error& e, SubscriptionId subId,
                        PublicationId pubId)
    {
        if (logLevel() <= LogLevel::error)
        {
            std::ostringstream oss;
            oss << "EVENT handler reported an error with URI=" << e.uri();
            if (!e.args().empty())
                oss << ", with Args=" << e.args();
            if (!e.kwargs().empty())
                oss << ", with ArgsKv=" << e.kwargs();
            oss << " (with subId=" << subId << ", pubId=" << pubId << ")";
            log(LogLevel::error, oss.str());
        }
    }

    void onInvocation(Message&& msg)
    {
        auto& invMsg = messageCast<InvocationMessage>(msg);
        auto requestId = invMsg.requestId();
        auto regId = invMsg.registrationId();

        auto kv = registry_.find(regId);
        if (kv == registry_.end())
        {
            peer_.sendError(WampMsgType::invocation, requestId,
                            {WampErrc::noSuchProcedure});
            log(LogLevel::warning,
                "No matching procedure for INVOCATION with registration ID "
                    + std::to_string(regId));
            return;
        }

        Invocation inv({}, shared_from_this(), userExecutor(),
                       std::move(invMsg));

        auto found = pendingInvocations_.find(requestId);
        if (found != pendingInvocations_.end())
        {
            auto err = Error(WampErrc::protocolViolation)
                           .withArgs("Request ID already in use");
            peer_.sendError(WampMsgType::invocation, requestId, std::move(err));
            log(LogLevel::warning,
                "Rejected INVOCATION with request ID "
                    + std::to_string(requestId) + " already in use");
            return;
        }

        const RegistrationRecord& rec = kv->second;
        pendingInvocations_.emplace(requestId, InvocationRecord{regId});
        postRpcRequest(rec.callSlot, std::move(inv));
    }

    void onInterrupt(Message&& msg)
    {
        auto& interruptMsg = messageCast<InterruptMessage>(msg);
        auto found = pendingInvocations_.find(interruptMsg.requestId());
        if (found == pendingInvocations_.end())
            return;

        InvocationRecord& rec = found->second;
        if (rec.interrupted)
            return;
        rec.interrupted = true;

        Interruption intr({}, shared_from_this(), userExecutor(),
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
            Error error{intr.reason().value_or("wamp.error.canceled")};
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

        auto exec = boost::asio::get_associated_executor(slot, userExecutor());
        Posted posted{shared_from_this(), std::move(slot), std::move(request)};
        boost::asio::post(executor(),
                          boost::asio::bind_executor(exec, std::move(posted)));
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
        bool ok = checkError(reply, handler);
        if (ok)
        {
            if (reply->type() == WampMsgType::error)
            {
                ok = false;
                auto& errMsg = messageCast<ErrorMessage>(*reply);
                const auto& uri = errMsg.uri();
                WampErrc errc = errorUriToCode(uri);
                bool hasArgs = !errMsg.args().empty() ||
                               !errMsg.kwargs().empty();
                if (errorPtr != nullptr)
                {
                    *errorPtr = Error({}, std::move(errMsg));
                }
                else if ((logLevel() <= LogLevel::error) &&
                         (errc == WampErrc::unknown || hasArgs))
                {
                    std::ostringstream oss;
                    oss << "Expected " << MessageTraits::lookup(type).name
                        << " reply but got ERROR with URI=" << uri;
                    if (!errMsg.args().empty())
                        oss << ", Args=" << errMsg.args();
                    if (!errMsg.kwargs().empty())
                        oss << ", ArgsKv=" << errMsg.kwargs();
                    log(LogLevel::error, oss.str());
                }

                dispatchHandler(handler, makeUnexpectedError(errc));
            }
            else
            {
                assert((reply->type() == type) &&
                       "Unexpected WAMP message type");
            }
        }
        return ok;
    }

    void checkReply(ErrorOr<Message>& reply, WampMsgType type)
    {
        std::string msgTypeName(MessageTraits::lookup(type).name);
        if (!reply.has_value())
        {
            if (logLevel() >= LogLevel::warning)
            {
                log(LogLevel::warning,
                    "Failure receiving reply for " + msgTypeName + " message",
                    reply.error());
            }
        }
        else if (reply->type() == WampMsgType::error)
        {
            if (logLevel() >= LogLevel::warning)
            {
                auto& msg = messageCast<ErrorMessage>(*reply);
                const auto& uri = msg.uri();
                std::ostringstream oss;
                oss << "Expected reply for " << msgTypeName
                    << " message but got ERROR with URI=" << uri;
                if (!msg.args().empty())
                    oss << ", Args=" << msg.args();
                if (!msg.kwargs().empty())
                    oss << ", ArgsKv=" << msg.kwargs();
                log(LogLevel::warning, oss.str());
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
    void dispatchHandler(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        if (!peer_.isTerminating())
        {
            dispatchVia(executor(), userExecutor(), std::move(handler),
                        std::forward<Ts>(args)...);
        }
    }

    template <typename S, typename... Ts>
    void dispatchHandler(const AnyReusableHandler<S>& handler, Ts&&... args)
    {
        if (!peer_.isTerminating())
        {
            dispatchVia(executor(), userExecutor(), handler,
                        std::forward<Ts>(args)...);
        }
    }

    template <typename S, typename... Ts>
    void complete(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        if (!peer_.isTerminating())
        {
            postVia(executor(), userExecutor(), std::move(handler),
                    std::forward<Ts>(args)...);
        }
    }

    template <typename S, typename... Ts>
    void completeNow(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        dispatchHandler(handler, std::forward<Ts>(args)...);
    }

    SlotId nextSlotId() {return nextSlotId_++;}

    Peer peer_;
    Connecting::Ptr currentConnector_;
    TopicMap exactTopics_;
    TopicMap prefixTopics_;
    TopicMap wildcardTopics_;
    Readership readership_;
    Registry registry_;
    InvocationMap pendingInvocations_;
    CallerTimeoutScheduler::Ptr timeoutScheduler_;
    ChallengeHandler challengeHandler_;
    SlotId nextSlotId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
