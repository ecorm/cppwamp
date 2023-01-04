/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_LOGGINGSESSIONIMPL_HPP
#define CPPWAMP_INTERNAL_LOGGINGSESSIONIMPL_HPP

#include <atomic>
#include <cassert>
#include <exception>
#include <future>
#include <sstream>
#include <memory>
#include <utility>
#include "../chits.hpp"
#include "../registration.hpp"
#include "../routerconfig.hpp"
#include "../subscription.hpp"
#include "callee.hpp"
#include "caller.hpp"
#include "callertimeout.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"
#include "subscriber.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class LocalSessionImpl : public std::enable_shared_from_this<LocalSessionImpl>,
                         public RouterSession,
                         public Callee, public Caller, public Subscriber
{
public:
    using Ptr                = std::shared_ptr<LocalSessionImpl>;
    using FutureErrorOrDone  = std::future<ErrorOrDone>;
    using EventSlot          = AnyReusableHandler<void (Event)>;
    using CallSlot           = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot      = AnyReusableHandler<Outcome (Interruption)>;
    using LogHandler         = AnyReusableHandler<void(LogEntry)>;
    using OngoingCallHandler = AnyReusableHandler<void(ErrorOr<Result>)>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(IoStrand s, AnyCompletionExecutor e)
    {
        return Ptr(new LocalSessionImpl(std::move(s), std::move(e)));
    }

    void join(RealmContext r, String realmUri, AuthInfo a)
    {
        realm_ = std::move(r);
        logger_ = realm_.logger();
        realm_.join(shared_from_this());
        a.join({}, std::move(realmUri), wampId());
        Base::setAuthInfo(std::move(a));
    }

    const IoStrand& strand() const {return strand_;}

    const AnyCompletionExecutor& userExecutor() const
    {
        return userExecutor_;
    }

    bool expired() const {return realm_.expired();}

    void kick(String hint, String reasonUri)
    {
        realm_.leave(wampId());
        // TODO: Cancel pending RPCS, clear subscriptions & registrations
    }

    Subscription subscribe(Topic&& topic, EventSlot&& slot)
    {
        // TODO
//        struct Requested
//        {
//            Ptr self;
//            SubscriptionRecord rec;
//            CompletionHandler<Subscription> handler;

//            void operator()(ErrorOr<Message> reply)
//            {
//                auto& me = *self;
//                if (me.checkReply(reply, WampMsgType::subscribed,
//                                  SessionErrc::subscribeError, handler))
//                {
//                    const auto& msg = messageCast<SubscribedMessage>(*reply);
//                    auto subId = msg.subscriptionId();
//                    auto slotId = me.nextSlotId();
//                    Subscription sub(self, subId, slotId, {});
//                    me.topics_.emplace(rec.topicUri, subId);
//                    me.readership_[subId][slotId] = std::move(rec);
//                    me.completeNow(handler, std::move(sub));
//                }
//            }
//        };

//        using std::move;
//        SubscriptionRecord rec = {topic.uri(), move(slot)};

//        auto kv = topics_.find(rec.topicUri);
//        if (kv == topics_.end())
//        {
//            peer_.request(
//                topic.message({}),
//                Requested{shared_from_this(), move(rec), move(handler)});
//        }
//        else
//        {
//            auto subId = kv->second;
//            auto slotId = nextSlotId();
//            Subscription sub{shared_from_this(), subId, slotId, {}};
//            readership_[subId][slotId] = move(rec);
//            complete(handler, move(sub));
//        }
        return {};
    }

    std::future<Subscription> safeSubscribe(Topic&& t, EventSlot&& s)
    {
        struct Dispatched
        {
            Ptr self;
            std::promise<Subscription> p;
            Topic t;
            EventSlot s;

            void operator()()
            {
                try
                {
                    p.set_value(self->subscribe(std::move(t), std::move(s)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        std::promise<Subscription> p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(std::move(p), std::move(t), std::move(s));
        return fut;
    }

    void unsubscribe(const Subscription& sub) override
    {
        // TODO
//        auto kv = readership_.find(sub.id());
//        if (kv != readership_.end())
//        {
//            auto& localSubs = kv->second;
//            if (!localSubs.empty())
//            {
//                auto subKv = localSubs.find(sub.slotId({}));
//                if (subKv != localSubs.end())
//                {
//                    if (localSubs.size() == 1u)
//                        topics_.erase(subKv->second.topicUri);

//                    localSubs.erase(subKv);
//                    if (localSubs.empty())
//                    {
//                        readership_.erase(kv);
//                        // TODO
//                        // sendUnsubscribe(sub.id());
//                    }
//                }
//            }
//        }
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

    PublicationId publish(Pub&& pub)
    {
        // TODO
//        if (state() != State::established)
//            return makeUnexpectedError(SessionErrc::invalidState);
//        return peer_.send(pub.message({}));
        return true;
    }

    std::future<PublicationId> safePublish(Pub&& pub)
    {
        struct Dispatched
        {
            Ptr self;
            std::promise<PublicationId> p;
            Pub pub;

            void operator()()
            {
                try
                {
                    p.set_value(self->publish(std::move(pub)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        std::promise<PublicationId> p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(std::move(p), std::move(pub));
        return fut;
    }

    Registration enroll(Procedure&& procedure, CallSlot&& callSlot,
                        InterruptSlot&& interruptSlot)
    {
        // TODO:
//        struct Requested
//        {
//            Ptr self;
//            RegistrationRecord rec;
//            CompletionHandler<Registration> handler;

//            void operator()(ErrorOr<Message> reply)
//            {
//                auto& me = *self;
//                if (me.checkReply(reply, WampMsgType::registered,
//                                  SessionErrc::registerError, handler))
//                {
//                    const auto& msg = messageCast<RegisteredMessage>(*reply);
//                    auto regId = msg.registrationId();
//                    Registration reg(self, regId, {});
//                    me.registry_[regId] = std::move(rec);
//                    me.completeNow(handler, std::move(reg));
//                }
//            }
//        };

//        if (!checkState(State::established, handler))
//            return;

//        using std::move;
//        RegistrationRecord rec{ move(callSlot), move(interruptSlot) };
//        peer_.request(procedure.message({}),
//                      Requested{shared_from_this(), move(rec), move(handler)});
        return {};
    }

    std::future<Registration> safeEnroll(Procedure&& proc, CallSlot&& c,
                                         InterruptSlot&& i)
    {
        using std::move;

        struct Dispatched
        {
            Ptr self;
            std::promise<Registration> p;
            Procedure proc;
            CallSlot c;
            InterruptSlot i;

            void operator()()
            {
                try
                {
                    p.set_value(self->enroll(move(proc), move(c), move(i)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        std::promise<Registration> p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(move(p), move(proc), move(c), move(i));
        return fut;
    }

    void unregister(const Registration& reg) override
    {
        // TODO
//        struct Requested
//        {
//            Ptr self;

//            void operator()(ErrorOr<Message> reply)
//            {
//                // Don't propagate WAMP errors, as we prefer this
//                // to be a no-fail cleanup operation.
//                self->checkReply(reply, WampMsgType::unregistered);
//            }
//        };

//        auto kv = registry_.find(reg.id());
//        if (kv != registry_.end())
//        {
//            registry_.erase(kv);
//            if (state() == State::established)
//            {
//                UnregisterMessage msg(reg.id());
//                peer_.request(msg, Requested{shared_from_this()});
//            }
//        }
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

    void oneShotCall(Rpc&& rpc, CallChit* chitPtr,
                     CompletionHandler<Result>&& handler)
    {
        // TODO
//        struct Requested
//        {
//            Ptr self;
//            Error* errorPtr;
//            CompletionHandler<Result> handler;

//            void operator()(ErrorOr<Message> reply)
//            {
//                auto& me = *self;
//                if (me.checkReply(reply, WampMsgType::result,
//                                  SessionErrc::callError, handler, errorPtr))
//                {
//                    auto& msg = messageCast<ResultMessage>(*reply);
//                    me.completeNow(handler, Result({}, std::move(msg)));
//                }
//            }
//        };

//        if (chitPtr)
//            *chitPtr = CallChit{};

//        if (!checkState(State::established, handler))
//            return;

//        auto cancelSlot =
//            boost::asio::get_associated_cancellation_slot(handler);
//        auto requestId = peer_.request(
//            rpc.message({}),
//            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
//        CallChit chit{shared_from_this(), requestId, rpc.cancelMode(), {}};

//        if (cancelSlot.is_connected())
//        {
//            cancelSlot.assign(
//                [chit](boost::asio::cancellation_type_t) {chit.cancel();});
//        }

//        if (rpc.callerTimeout().count() != 0)
//            timeoutScheduler_->add(rpc.callerTimeout(), requestId);

//        if (chitPtr)
//            *chitPtr = chit;
    }

    void safeOneShotCall(Rpc&& r, CallChit* c, CompletionHandler<Result>&& f)
    {
        using std::move;

        struct Dispatched
        {
            Ptr self;
            Rpc r;
            CallChit* c;
            CompletionHandler<Result> f;
            void operator()() {self->oneShotCall(move(r), c, move(f));}
        };

        safelyDispatch<Dispatched>(move(r), c, move(f));
    }

    void ongoingCall(Rpc&& rpc, CallChit* chitPtr, OngoingCallHandler&& handler)
    {
        // TODO
//        struct Requested
//        {
//            Ptr self;
//            Error* errorPtr;
//            OngoingCallHandler handler;

//            void operator()(ErrorOr<Message> reply)
//            {
//                auto& me = *self;
//                if (me.checkReply(reply, WampMsgType::result,
//                                  SessionErrc::callError, handler, errorPtr))
//                {
//                    auto& resultMsg = messageCast<ResultMessage>(*reply);
//                    me.dispatchHandler(handler,
//                                       Result({}, std::move(resultMsg)));
//                }
//            }
//        };

//        if (chitPtr)
//            *chitPtr = CallChit{};

//        if (!checkState(State::established, handler))
//            return;

//        rpc.withProgressiveResults(true);

//        auto cancelSlot =
//            boost::asio::get_associated_cancellation_slot(handler);
//        auto requestId = peer_.ongoingRequest(
//            rpc.message({}),
//            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
//        CallChit chit{shared_from_this(), requestId, rpc.cancelMode(), {}};

//        if (cancelSlot.is_connected())
//        {
//            cancelSlot.assign(
//                [chit](boost::asio::cancellation_type_t) {chit.cancel();});
//        }

//        if (rpc.callerTimeout().count() != 0)
//            timeoutScheduler_->add(rpc.callerTimeout(), requestId);

//        if (chitPtr)
//            *chitPtr = chit;
    }

    void safeOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f)
    {
        using std::move;

        struct Dispatched
        {
            Ptr self;
            Rpc r;
            CallChit* c;
            OngoingCallHandler f;
            void operator()() {self->ongoingCall(move(r), c, move(f));}
        };

        safelyDispatch<Dispatched>(move(r), c, move(f));
    }

    ErrorOrDone cancelCall(RequestId reqId, CallCancelMode mode) override
    {
        // TODO
//        if (state() != State::established)
//            return makeUnexpectedError(SessionErrc::invalidState);
//        return peer_.cancelCall(CallCancellation{reqId, mode});
        return true;
    }

    std::future<ErrorOrDone> safeCancelCall(RequestId r,
                                            CallCancelMode m) override
    {
        struct Dispatched
        {
            Ptr self;
            ErrorOrDonePromise p;
            RequestId r;
            CallCancelMode m;

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
        safelyDispatch<Dispatched>(std::move(p), r, m);
        return fut;
    }

    ErrorOrDone yield(RequestId reqId, Result&& result) override
    {
        // TODO
//        if (state() != State::established)
//            return makeUnexpectedError(SessionErrc::invalidState);

//        if (!result.isProgressive())
//            pendingInvocations_.erase(reqId);
//        auto done = peer_.send(result.yieldMessage({}, reqId));
//        if (done == makeUnexpectedError(SessionErrc::payloadSizeExceeded))
//            yield(reqId, Error("wamp.error.payload_size_exceeded"));
//        return done;
        return true;
    }

    FutureErrorOrDone safeYield(RequestId i, Result&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            ErrorOrDonePromise p;
            Result r;
            RequestId i;

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
        safelyDispatch<Dispatched>(std::move(p), std::move(r), i);
        return fut;
    }

    ErrorOrDone yield(RequestId reqId, Error&& error) override
    {
        // TODO
//        if (state() != State::established)
//            return makeUnexpectedError(SessionErrc::invalidState);

//        pendingInvocations_.erase(reqId);
//        return peer_.sendError(WampMsgType::invocation, reqId,
//                               std::move(error));
        return true;
    }

    FutureErrorOrDone safeYield(RequestId i, Error&& e) override
    {
        struct Dispatched
        {
            Ptr self;
            ErrorOrDonePromise p;
            RequestId i;
            Error e;

            void operator()()
            {
                try
                {
                    p.set_value(self->yield(i, std::move(e)));
                }
                catch (...)
                {
                    p.set_exception(std::current_exception());
                }
            }
        };

        ErrorOrDonePromise p;
        auto fut = p.get_future();
        safelyDispatch<Dispatched>(std::move(p), i, std::move(e));
        return fut;
    }

    void close(bool terminate, Reason r) override
    {
        // TODO
    }

    void sendInvocation(Invocation&&) override {}

    void sendError(Error&&) override {}

    void sendResult(Result&&) override {}

    void sendInterruption(Interruption&&) override {}

    void log(LogEntry&& e) override
    {
        // TODO
//        e.append(logSuffix_);
//        logger_->log(std::move(e));
    }

    void logAccess(AccessActionInfo&& i) override
    {
        // TODO
//        logger_->log(AccessLogEntry{sessionInfo_, std::move(i)});
    }

private:
    using Base = RouterSession;
    using ErrorOrDonePromise = std::promise<ErrorOrDone>;

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

    using Message        = WampMessage;
    using SlotId         = uint64_t;
    using LocalSubs      = std::map<SlotId, SubscriptionRecord>;
    using Readership     = std::map<SubscriptionId, LocalSubs>;
    using TopicMap       = std::map<std::string, SubscriptionId>;
    using Registry       = std::map<RegistrationId, RegistrationRecord>;
    using InvocationMap  = std::map<RequestId, RegistrationId>;
    using CallerTimeoutDuration = typename Rpc::CallerTimeoutDuration;

    LocalSessionImpl(IoStrand s, AnyCompletionExecutor e)
        : strand_(std::move(s)),
          userExecutor_(std::move(e)),
          timeoutScheduler_(CallerTimeoutScheduler::create(strand_))
    {}

    template <typename F, typename... Ts>
    void safelyDispatch(Ts&&... args)
    {
        boost::asio::dispatch(
            strand(), F{shared_from_this(), std::forward<Ts>(args)...});
    }

    void onInbound(Message msg)
    {
        switch (msg.type())
        {
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
        boost::asio::post(exec, Posted{shared_from_this(), sub.slot, event});
    }

    void warnEventError(const Error& e, SubscriptionId subId,
                        PublicationId pubId)
    {
        if (logLevel() <= LogLevel::error)
        {
            std::ostringstream oss;
            oss << "EVENT handler reported an error: "
                << e.args()
                << " (with subId=" << subId
                << " pubId=" << pubId << ")";
            log(LogLevel::error, oss.str());
        }
    }

    void onInvocation(Message&& msg)
    {
        auto& invMsg = messageCast<InvocationMessage>(msg);
        auto requestId = invMsg.requestId();
        auto regId = invMsg.registrationId();

        auto kv = registry_.find(regId);
        if (kv != registry_.end())
        {
            const RegistrationRecord& rec = kv->second;
            Invocation inv({}, shared_from_this(), userExecutor(),
                           std::move(invMsg));
            pendingInvocations_[requestId] = regId;
            postRpcRequest(rec.callSlot, std::move(inv));
        }
        else
        {
            // TODO
//            peer_.sendError(WampMsgType::invocation, requestId,
//                            Error("wamp.error.no_such_procedure"));
            log(LogLevel::warning,
                "No matching procedure for INVOCATION with registration ID "
                    + std::to_string(regId));
        }
    }

    void onInterrupt(Message&& msg)
    {
        auto& interruptMsg = messageCast<InterruptMessage>(msg);
        auto found = pendingInvocations_.find(interruptMsg.requestId());
        if (found != pendingInvocations_.end())
        {
            auto registrationId = found->second;
            pendingInvocations_.erase(found);
            auto kv = registry_.find(registrationId);
            if ((kv != registry_.end()) &&
                (kv->second.interruptSlot != nullptr))
            {
                const RegistrationRecord& rec = kv->second;
                using std::move;
                Interruption intr({}, shared_from_this(), userExecutor(),
                                  move(interruptMsg));
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
                    me.yield(requestId, Error(e)).value();
                }
            }
        };

        auto exec = boost::asio::get_associated_executor(slot, userExecutor());
        boost::asio::post(
            exec,
            Posted{shared_from_this(), move(slot), move(request)});
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
                    SessionErrc defaultErrc, THandler& handler,
                    Error* errorPtr = nullptr)
    {
        bool ok = checkError(reply, handler);
        if (ok)
        {
            if (reply->type() == WampMsgType::error)
            {
                ok = false;
                auto& errMsg = messageCast<ErrorMessage>(*reply);
                const auto& uri = errMsg.reasonUri();
                SessionErrc errc;
                bool found = errorUriToCode(uri, defaultErrc, errc);
                bool hasArgs = !errMsg.args().empty() ||
                               !errMsg.kwargs().empty();
                if (errorPtr != nullptr)
                {
                    *errorPtr = Error({}, std::move(errMsg));
                }
                else if ((logLevel() <= LogLevel::error) && (!found || hasArgs))
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
                const auto& uri = msg.reasonUri();
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

    LogLevel logLevel() const {return logger_->level();}

    void log(LogLevel severity, std::string message, std::error_code ec = {})
    {
        logger_->log({severity, std::move(message), ec});
    }

    template <typename S, typename... Ts>
    void dispatchHandler(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        if (!isTerminating())
        {
            dispatchVia(userExecutor(), std::move(handler),
                        std::forward<Ts>(args)...);
        }
    }

    template <typename S, typename... Ts>
    void dispatchHandler(const AnyReusableHandler<S>& handler, Ts&&... args)
    {
        if (!isTerminating())
            dispatchVia(userExecutor(), handler, std::forward<Ts>(args)...);
    }

    template <typename S, typename... Ts>
    void complete(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        if (!isTerminating())
        {
            postVia(userExecutor(), std::move(handler),
                    std::forward<Ts>(args)...);
        }
    }

    template <typename S, typename... Ts>
    void completeNow(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        dispatchHandler(handler, std::forward<Ts>(args)...);
    }

    bool isTerminating() const {return isTerminating_.load();}

    SlotId nextSlotId() {return nextSlotId_++;}

    IoStrand strand_;
    AnyCompletionExecutor userExecutor_;
    RealmContext realm_;
    TopicMap topics_;
    Readership readership_;
    Registry registry_;
    InvocationMap pendingInvocations_;
    RouterLogger::Ptr logger_;
    CallerTimeoutScheduler::Ptr timeoutScheduler_;
    std::atomic<bool> isTerminating_;
    SlotId nextSlotId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_LOGGINGSESSIONIMPL_HPP
