/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENT_HPP
#define CPPWAMP_INTERNAL_CLIENT_HPP

#include <atomic>
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
#include "../codec.hpp"
#include "../connector.hpp"
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../transport.hpp"
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
class Client : public ClientInterface, public Peer
{
public:
    using Ptr          = std::shared_ptr<Client>;
    using WeakPtr      = std::weak_ptr<Client>;
    using TransportPtr = Transporting::Ptr;
    using State        = SessionState;

    using LogHandler         = AnyReusableHandler<void(std::string)>;
    using StateChangeHandler = AnyReusableHandler<void(SessionState)>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static Ptr create(AnyIoExecutor exec)
    {
        return Ptr(new Client(std::move(exec)));
    }

    static Ptr create(const AnyIoExecutor& exec, AnyCompletionExecutor userExec)
    {
        return Ptr(new Client(exec, std::move(userExec)));
    }

    State state() const override {return Peer::state();}

    const IoStrand& strand() const override {return Peer::strand();}

    const AnyCompletionExecutor& userExecutor() const override
    {
        return Peer::userExecutor();
    }

    void setWarningHandler(LogHandler handler) override
    {
        warningHandler_ = std::move(handler);
    }

    void safeSetWarningHandler(LogHandler f) override
    {
        struct Dispatched
        {
            Ptr self;
            LogHandler f;
            void operator()() {self->setWarningHandler(std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(f));
    }

    void setTraceHandler(LogHandler handler) override
    {
        Peer::setTraceHandler(std::move(handler));
    }

    void safeSetTraceHandler(LogHandler f) override
    {
        struct Dispatched
        {
            Ptr self;
            LogHandler f;
            void operator()() {self->setTraceHandler(std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(f));
    }

    void setStateChangeHandler(StateChangeHandler handler) override
    {
        Peer::setStateChangeHandler(std::move(handler));
    }

    void safeSetStateChangeHandler(StateChangeHandler f) override
    {
        struct Dispatched
        {
            Ptr self;
            StateChangeHandler f;
            void operator()() {self->setStateChangeHandler(std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(f));
    }

    void setChallengeHandler(ChallengeHandler handler) override
    {
        challengeHandler_ = std::move(handler);
    }

    void safeSetChallengeHandler(ChallengeHandler f) override
    {
        struct Dispatched
        {
            Ptr self;
            ChallengeHandler f;
            void operator()() {self->setChallengeHandler(std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(f));
    }

    void connect(ConnectionWishList wishes,
                 CompletionHandler<size_t>&& handler) override
    {
        assert(!wishes.empty());

        if (!checkState(State::disconnected, handler))
            return;

        setTerminating(false);
        setState(State::connecting);
        currentConnector_ = nullptr;

        // This makes it easier to transport the move-only completion handler
        // through the gauntlet of intermediary handler functions.
        auto sharedHandler =
            std::make_shared<CompletionHandler<size_t>>(std::move(handler));

        doConnect(std::move(wishes), 0, std::move(sharedHandler));
    }

    void safeConnect(ConnectionWishList w,
                     CompletionHandler<size_t>&& f) override
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

        if (!checkState(State::closed, handler))
            return;

        realm.withOption("agent", Version::agentString())
             .withOption("roles", roles());
        Peer::start();
        Peer::request(realm.message({}),
                      Requested{shared_from_this(), std::move(handler),
                                realm.uri(), realm.abort({})});
    }

    void safeJoin(Realm&& r, CompletionHandler<SessionInfo>&& f) override
    {
        struct Dispatched
        {
            Ptr self;
            Realm r;
            CompletionHandler<SessionInfo> f;
            void operator()() {self->join(std::move(r), std::move(f));}
        };

        safelyDispatch<Dispatched>(std::move(r), std::move(f));
    }

    void authenticate(Authentication&& auth) override
    {
        if (state() != State::authenticating)
        {
            warn("Authentication message discarded called while not "
                 "in the authenticating state");
            return;
        }
        Peer::send(auth.message({}));
    }

    void safeAuthenticate(Authentication&& a) override
    {
        struct Dispatched
        {
            Ptr self;
            Authentication a;
            void operator()() {self->authenticate(std::move(a));}
        };

        safelyDispatch<Dispatched>(std::move(a));
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
                me.topics_.clear();
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

        if (!checkState(State::established, handler))
            return;
        timeoutScheduler_->clear();
        Peer::adjourn(reason,
                      Adjourned{shared_from_this(), std::move(handler)});
    }

    void safeLeave(Reason&& r, CompletionHandler<Reason>&& f) override
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

    void disconnect() override
    {
        doDisconnect();
    }

    void safeDisconnect() override
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->disconnect();}
        };

        safelyDispatch<Dispatched>();
    }

    void terminate() override
    {
        setTerminating(true);
        doDisconnect();
    }

    void safeTerminate() override
    {
        struct Dispatched
        {
            Ptr self;
            void operator()() {self->terminate();}
        };

        safelyDispatch<Dispatched>();
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

        if (!checkState(State::established, handler))
            return;

        using std::move;
        SubscriptionRecord rec = {topic.uri(), move(slot)};

        auto kv = topics_.find(rec.topicUri);
        if (kv == topics_.end())
        {
            auto self = this->shared_from_this();
            Peer::request(
                topic.message({}),
                Requested{shared_from_this(), move(rec), move(handler)});
        }
        else
        {
            auto subId = kv->second;
            auto slotId = nextSlotId();
            Subscription sub{this->shared_from_this(), subId, slotId, {}};
            readership_[subId][slotId] = move(rec);
            postUserHandler(handler, move(sub));
        }
    }

    void safeSubscribe(Topic&& t, EventSlot&& s,
                       CompletionHandler<Subscription>&& f) override
    {
        using std::move;

        struct Dispatched
        {
            Ptr self;
            Topic t;
            EventSlot s;
            CompletionHandler<Subscription> f;
            void operator()() {self->subscribe(move(t), move(s), move(f));}
        };

        safelyDispatch<Dispatched>(move(t), move(s), move(f));
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

    void unsubscribe(const Subscription& sub,
                     CompletionHandler<bool>&& handler) override
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
                        sendUnsubscribe(sub.id(), std::move(handler));
                    }
                }
            }
        }
        else
        {
            postUserHandler(handler, false);
        }
    }

    void safeUnsubscribe(const Subscription& s,
                         CompletionHandler<bool>&& f) override
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

    void publish(Pub&& pub) override
    {
        if (state() != State::established)
        {
            warn("Publish message discarded while not established");
            return;
        }
        Peer::send(pub.message({}));
    }

    void safePublish(Pub&& p) override
    {
        struct Dispatched
        {
            Ptr self;
            Pub p;
            void operator()() {self->publish(std::move(p));}
        };

        safelyDispatch<Dispatched>(std::move(p));
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

        if (!checkState(State::established, handler))
            return;

        pub.withOption("acknowledge", true);
        auto self = this->shared_from_this();
        Peer::request(pub.message({}),
                      Requested{shared_from_this(), std::move(handler)});
    }

    void safePublish(Pub&& p, CompletionHandler<PublicationId>&& f) override
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

        if (!checkState(State::established, handler))
            return;

        using std::move;
        RegistrationRecord rec{ move(callSlot), move(interruptSlot) };
        auto self = this->shared_from_this();
        Peer::request(procedure.message({}),
                      Requested{shared_from_this(), move(rec), move(handler)});
    }

    void safeEnroll(Procedure&& p, CallSlot&& c, InterruptSlot&& i,
                    CompletionHandler<Registration>&& f) override
    {
        using std::move;

        struct Dispatched
        {
            Ptr self;
            Procedure p;
            CallSlot c;
            InterruptSlot i;
            CompletionHandler<Registration> f;

            void operator()()
            {
                self->enroll(move(p), move(c), move(i), move(f));
            }
        };

        safelyDispatch<Dispatched>(move(p), move(c), move(i), move(f));
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
                Peer::request(msg, Requested{shared_from_this()});
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

        auto kv = registry_.find(reg.id());
        if (kv != registry_.end())
        {
            registry_.erase(kv);
            auto self = this->shared_from_this();
            UnregisterMessage msg(reg.id());
            if (state() == State::established)
            {
                Peer::request(msg, Requested{shared_from_this(),
                                             std::move(handler)});
            }
            else
            {
                warn("Unregister message discarded while not established");
                postUserHandler(handler, true);
            }
        }
        else
        {
            postUserHandler(handler, false);
        }
    }

    void safeUnregister(const Registration& r,
                        CompletionHandler<bool>&& f) override
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

        if (chitPtr)
            *chitPtr = CallChit{};

        if (!checkState(State::established, handler))
            return;

        auto self = this->shared_from_this();
        auto cancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = Peer::request(
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

    void safeOneShotCall(Rpc&& r, CallChit* c,
                         CompletionHandler<Result>&& f) override
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

    void ongoingCall(Rpc&& rpc, CallChit* chitPtr,
                     OngoingCallHandler&& handler) override
    {
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

        if (chitPtr)
            *chitPtr = CallChit{};

        if (!checkState(State::established, handler))
            return;

        rpc.withProgressiveResults(true);

        auto cancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto requestId = Peer::ongoingRequest(
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

    void safeOngoingCall(Rpc&& r, CallChit* c, OngoingCallHandler&& f) override
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

    void cancelCall(RequestId reqId, CallCancelMode mode) override
    {
        if (state() != State::established)
        {
            warn("Cancel RPC message discarded while not established");
            return;
        }
        Peer::cancelCall(CallCancellation{reqId, mode});
    }

    void safeCancelCall(RequestId r, CallCancelMode m) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            CallCancelMode m;
            void operator()() {self->cancelCall(r, m);}
        };

        safelyDispatch<Dispatched>(r, m);
    }

    void yield(RequestId reqId, Result&& result) override
    {
        if (state() != State::established)
        {
            warn("Yield message discarded while not established");
            return;
        }

        if (!result.isProgressive())
            pendingInvocations_.erase(reqId);
        Peer::send(result.yieldMessage({}, reqId));
    }

    void safeYield(RequestId i, Result&& r) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId i;
            Result r;
            void operator()() {self->yield(i, std::move(r));}
        };

        safelyDispatch<Dispatched>(i, std::move(r));
    }

    void yield(RequestId reqId, Error&& error) override
    {
        if (state() != State::established)
        {
            warn("Yield message discarded while not established");
            return;
        }

        pendingInvocations_.erase(reqId);
        Peer::sendError(WampMsgType::invocation, reqId, std::move(error));
    }

    void safeYield(RequestId r, Error&& e) override
    {
        struct Dispatched
        {
            Ptr self;
            RequestId r;
            Error e;
            void operator()() {self->yield(r, std::move(e));}
        };

        safelyDispatch<Dispatched>(r, std::move(e));
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

    using Base           = Peer;
    using WampMsgType    = internal::WampMsgType;
    using Message        = internal::WampMessage;
    using SlotId         = uint64_t;
    using LocalSubs      = std::map<SlotId, SubscriptionRecord>;
    using Readership     = std::map<SubscriptionId, LocalSubs>;
    using TopicMap       = std::map<std::string, SubscriptionId>;
    using Registry       = std::map<RegistrationId, RegistrationRecord>;
    using InvocationMap  = std::map<RequestId, RegistrationId>;
    using CallerTimeoutDuration = typename Rpc::CallerTimeoutDuration;

    using Peer::userExecutor;

    Client(AnyIoExecutor exec)
        : Base(std::move(exec)),
          timeoutScheduler_(CallerTimeoutScheduler::create(Base::strand()))
    {}

    Client(const AnyIoExecutor& exec, AnyCompletionExecutor userExec)
        : Base(exec, std::move(userExec)),
          timeoutScheduler_(CallerTimeoutScheduler::create(Base::strand()))
    {}

    Ptr shared_from_this()
    {
        return std::static_pointer_cast<Client>( Peer::shared_from_this() );
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
            auto unex = makeUnexpectedError(SessionErrc::invalidState);
            if (!isTerminating())
                postVia(userExecutor(), std::move(handler), std::move(unex));
        }
        return valid;
    }

    void doConnect(ConnectionWishList&& wishes, size_t index,
                   std::shared_ptr<CompletionHandler<size_t>> handler)
    {
        using std::move;
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
                if (me.isTerminating())
                    return;

                if (!transport)
                {
                    me.onConnectFailure(move(wishes), index, transport.error(),
                                        move(handler));
                }
                else if (me.state() == State::connecting)
                {
                    auto codec = wishes.at(index).makeCodec();
                    me.open(std::move(*transport), std::move(codec));
                    me.dispatchUserHandler(*handler, index);
                }
                else
                {
                    auto ec = make_error_code(TransportErrc::aborted);
                    me.postUserHandler(*handler, UnexpectedError(ec));
                }
            }
        };

        currentConnector_ = wishes.at(index).makeConnector(strand());
        currentConnector_->establish(
            Established{shared_from_this(), move(wishes), index, move(handler)});
    }

    void onConnectFailure(ConnectionWishList&& wishes, size_t index,
                          std::error_code ec,
                          std::shared_ptr<CompletionHandler<size_t>> handler)
    {
        if (ec == TransportErrc::aborted)
        {
            dispatchUserHandler(*handler, UnexpectedError(ec));
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
                setState(State::failed);
                if (wishes.size() > 1)
                    ec = make_error_code(SessionErrc::allTransportsFailed);
                dispatchUserHandler(*handler, UnexpectedError(ec));
            }
        }
    }

    void doDisconnect()
    {
        if (Peer::state() == State::connecting)
            currentConnector_->cancel();

        topics_.clear();
        readership_.clear();
        registry_.clear();
        pendingInvocations_.clear();
        timeoutScheduler_->clear();
        Peer::close();
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
            Peer::request(msg, Requested{shared_from_this()});
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

        if (state() != State::established)
        {
            warn("Unsubscribe message discarded while not established");
            postUserHandler(handler, true);
            return;
        }

        auto self = this->shared_from_this();
        UnsubscribeMessage msg(subId);
        Peer::request(msg, Requested{shared_from_this(), std::move(handler)});
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
            Peer::sendError(WampMsgType::invocation, requestId,
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
    void postUserHandler(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        if (!isTerminating())
        {
            postVia(userExecutor(), std::move(handler),
                    std::forward<Ts>(args)...);
        }
    }

    template <typename S, typename... Ts>
    void dispatchUserHandler(AnyCompletionHandler<S>& handler, Ts&&... args)
    {
        if (!isTerminating())
        {
            dispatchVia(userExecutor(), std::move(handler),
                        std::forward<Ts>(args)...);
        }
    }

    template <typename S, typename... Ts>
    void dispatchUserHandler(const AnyReusableHandler<S>& handler, Ts&&... args)
    {
        if (!isTerminating())
            dispatchVia(userExecutor(), handler, std::forward<Ts>(args)...);
    }

    SlotId nextSlotId() {return nextSlotId_++;}

    Connecting::Ptr currentConnector_;
    TopicMap topics_;
    Readership readership_;
    Registry registry_;
    InvocationMap pendingInvocations_;
    CallerTimeoutScheduler::Ptr timeoutScheduler_;
    LogHandler warningHandler_;
    ChallengeHandler challengeHandler_;
    SlotId nextSlotId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENT_HPP
