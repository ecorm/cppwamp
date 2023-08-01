/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENT_HPP
#define CPPWAMP_INTERNAL_CLIENT_HPP

#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include "boost/asio/steady_timer.hpp"
#include "../any.hpp"
#include "../anyhandler.hpp"
#include "../calleestreaming.hpp"
#include "../callerstreaming.hpp"
#include "../clientinfo.hpp"
#include "../connector.hpp"
#include "../errorinfo.hpp"
#include "../features.hpp"
#include "../pubsubinfo.hpp"
#include "../registration.hpp"
#include "../rpcinfo.hpp"
#include "../subscription.hpp"
#include "../transport.hpp"
#include "../version.hpp"
#include "clientcontext.hpp"
#include "commandinfo.hpp"
#include "procedureregistry.hpp"
#include "readership.hpp"
#include "requestor.hpp"
#include "peer.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Provides the WAMP client implementation.
//------------------------------------------------------------------------------
class Client final : public std::enable_shared_from_this<Client>,
                     public ClientLike, private PeerListener
{
public:
    using Ptr             = std::shared_ptr<Client>;
    using TransportPtr    = Transporting::Ptr;
    using State           = SessionState;
    using TimeoutDuration = std::chrono::steady_clock::duration;
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
        return Ptr(new Client(std::move(peer), std::move(exec)));
    }

    ~Client() override = default;

    State state() const {return peer_->state();}

    const AnyIoExecutor& executor() const {return executor_;}

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

    // NOLINTNEXTLINE(bugprone-exception-escape)
    void disconnect() noexcept
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

    void unsubscribe(Subscription s, TimeoutDuration t,
                     CompletionHandler<bool>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Subscription s;
            TimeoutDuration t;
            CompletionHandler<bool> f;
            void operator()() {self->doUnsubscribe(s, t, std::move(f));}
        };

        s.disarm({});
        safelyDispatch<Dispatched>(std::move(s), t, std::move(f));
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

    void unregister(Registration r, TimeoutDuration t,
                    CompletionHandler<bool>&& f)
    {
        struct Dispatched
        {
            Ptr self;
            Registration r;
            TimeoutDuration t;
            CompletionHandler<bool> f;
            void operator()() {self->doUnregister(r, t, std::move(f));}
        };

        r.disarm({});
        safelyDispatch<Dispatched>(std::move(r), t, std::move(f));
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

    Client(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(const Client&) = delete;
    Client& operator=(Client&&) = delete;

private:
    using RequestKey      = typename Message::RequestKey;
    using RequestHandler  = AnyCompletionHandler<void (ErrorOr<Message>)>;

    Client(Peer::Ptr peer, AnyIoExecutor exec)
        : executor_(std::move(exec)),
          strand_(boost::asio::make_strand(executor_)),
          peer_(std::move(peer)),
          readership_(executor_),
          registry_(peer_.get(), executor_),
          requestor_(peer_.get(), strand_, executor_),
          connectionTimer_(strand_)
    {
        peer_->listen(this);
    }

    void removeSlot(SubscriptionTag, SubscriptionKey key) override
    {
        struct Dispatched
        {
            Ptr self;
            SubscriptionKey key;
            void operator()() {self->doUnsubscribe(key);}
        };

        safelyDispatch<Dispatched>(key);
    }

    void removeSlot(RegistrationTag, RegistrationKey key) override
    {
        struct Dispatched
        {
            Ptr self;
            RegistrationKey key;
            void operator()() {self->doUnregister(key);}
        };

        safelyDispatch<Dispatched>(key);
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

    void onPeerAuthenticate(Authentication&&) override
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
                catch (Reason& r)
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
        const Event event{{}, std::move(msg)};
        const bool ok = readership_.onEvent(event);
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
        peer_->send(Error{PassKey{}, MessageKind::invocation, reqId, ec});
    }

    void onInvocationProgressNotAllowed(RequestId reqId, RegistrationId regId)
    {
        std::string why{"Router requested progress on an RPC endpoint not "
                        "registered as a stream"};
        auto ec = make_error_code(WampErrc::optionNotAllowed);
        report({IncidentKind::trouble, ec,
                why + ", with registration ID " + std::to_string(regId)});
        Error error(PassKey{}, MessageKind::invocation, reqId, ec);
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
        assert(msg.isReply());
        const char* msgName = msg.name();
        auto kind = msg.kind();
        if (!requestor_.onReply(std::move(msg)))
        {
            // Ignore spurious RESULT and ERROR responses that can occur
            // due to race conditions.
            using K = MessageKind;
            if ((kind != K::result) && (kind != K::error))
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
        const WampErrc errc = errorUriToCode(uri);

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
                me.connectionTimer_.cancel();
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

        auto& wish = wishes.at(index);
        currentConnector_ = wish.makeConnector(strand_);
        currentConnector_->establish(
            Established{shared_from_this(), std::move(wishes), index,
                        std::move(handler)});

        if (wish.timeout().count() != 0)
        {
            auto self = shared_from_this();
            connectionTimer_.expires_after(wish.timeout());
            connectionTimer_.async_wait(
                [self](boost::system::error_code ec)
                {
                    if (!ec && self->currentConnector_ != nullptr)
                        self->currentConnector_->cancel();
                });
        }
    }

    void onConnectFailure(ConnectionWishList&& wishes, size_t index,
                          std::error_code ec,
                          std::shared_ptr<CompletionHandler<size_t>> handler)
    {
        // TODO: report intermediate connection failures as incidents
        if ((ec == TransportErrc::aborted) && state() != State::connecting)
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
                else if (ec == TransportErrc::aborted)
                    ec = make_error_code(TransportErrc::timeout);
                peer_->failConnecting();
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
        auto timeout = realm.timeout();
        request(std::move(realm), timeout, std::move(requested));
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
        auto oldState = state();
        peer_->disconnect();
        if (oldState == State::connecting)
            currentConnector_->cancel();
        clear();
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
                const Subscribed ack{std::move(*reply)};
                auto sub = me.readership_.createSubscription(
                    ack.subscriptionId(), std::move(matchUri), std::move(slot),
                    me.makeContext());
                me.completeNow(handler, std::move(sub));
            }
        };

        if (!checkState(State::established, handler))
            return;

        MatchUri matchUri{topic};
        auto* record = readership_.findSubscription(matchUri);
        if (record != nullptr)
        {
            auto subscription = readership_.addSubscriber(
                *record, std::move(slot), makeContext());
            complete(handler, std::move(subscription));
        }
        else
        {
            Requested requested{shared_from_this(), std::move(matchUri),
                                std::move(slot), std::move(handler)};
            auto timeout = topic.timeout();
            request(std::move(topic), timeout, std::move(requested));
        }
    }

    void doUnsubscribe(SubscriptionKey key)
    {
        if (readership_.unsubscribe(key))
            sendUnsubscribe(key.first);
    }

    void doUnsubscribe(const Subscription& sub, TimeoutDuration timeout,
                       CompletionHandler<bool>&& handler)
    {
        if (!sub || !readership_.unsubscribe(sub.key({})))
            return complete(handler, false);
        sendUnsubscribe(sub.id(), timeout, std::move(handler));
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
                    const Published ack{std::move(*reply)};
                    me.completeNow(handler, ack.publicationId());
                }
            }
        };

        if (!checkState(State::established, handler))
            return;

        pub.withOption("acknowledge", true);
        auto timeout = pub.timeout();
        request(std::move(pub), timeout,
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
                const Registered ack{std::move(*reply)};
                r.setRegistrationId(ack.registrationId());
                auto reg = me.registry_.enroll(std::move(r));
                me.completeNow(f, std::move(reg));
            }
        };

        if (!checkState(State::established, f))
            return;

        ProcedureRegistration reg{std::move(c), std::move(i), p.uri(),
                                  makeContext()};
        auto timeout = p.timeout();
        request(std::move(p), timeout,
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
                const Registered ack{std::move(*reply)};
                r.setRegistrationId(ack.registrationId());
                auto reg = me.registry_.enroll(std::move(r));
                me.completeNow(f, std::move(reg));
            }
        };

        if (!checkState(State::established, f))
            return;

        StreamRegistration reg{std::move(ss), s.uri(), makeContext(),
                               s.invitationExpected()};
        request(std::move(s),
                Requested{shared_from_this(), std::move(reg), std::move(f)});
    }

    void doUnregister(RegistrationId regId)
    {
        struct Requested
        {
            void operator()(const ErrorOr<Message>&)
            {
                // Don't propagate WAMP errors, as we prefer this
                // to be a no-fail cleanup operation.
            }
        };

        if (registry_.unregister(regId) && state() == State::established)
            request(Unregister{regId}, Requested{});
    }

    void doUnregister(const Registration& reg, TimeoutDuration t,
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

        if (!reg || !registry_.unregister(reg.id()))
            return complete(handler, false);

        if (checkState(State::established, handler))
        {
            request(Unregister{reg.id()}, t,
                    Requested{shared_from_this(), std::move(handler)});
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

        auto rpcCancelSlot = rpc.cancellationSlot({});
        auto boundCancelSlot =
            boost::asio::get_associated_cancellation_slot(handler);
        auto mode = rpc.cancelMode();

        auto requestId = requestor_.request(
            std::move(rpc),
            rpc.callerTimeout(),
            Requested{shared_from_this(), rpc.error({}), std::move(handler)});
        if (!requestId)
            return;

        if (rpcCancelSlot.is_connected())
        {
            rpcCancelSlot.emplace(makeContext(), *requestId);
        }
        else if (boundCancelSlot.is_connected())
        {
            auto self = shared_from_this();
            auto reqId = *requestId;
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
        auto reqId = chunk.requestId({});
        auto done = registry_.yield(std::move(chunk));
        if (incidentSlot_ && !done)
        {
            std::ostringstream oss;
            oss << "Stream RESULT with requestId=" << reqId
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
        auto reqId = result.requestId({});
        auto done = registry_.yield(std::move(result));
        if (incidentSlot_ && !done)
        {
            std::ostringstream oss;
            oss << "RPC RESULT with requestId=" << reqId
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
        auto reqId = error.requestId({});
        auto done = registry_.yield(std::move(error));
        if (incidentSlot_ && !done)
        {
            std::ostringstream oss;
            oss << "INVOCATION ERROR with requestId=" << reqId
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
        const bool valid = state() == expectedState;
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

    template <typename C>
    ErrorOr<RequestId> request(C&& command, RequestHandler&& handler)
    {
        return requestor_.request(std::forward<C>(command), std::move(handler));
    }

    template <typename C>
    ErrorOr<RequestId> request(C&& command, TimeoutDuration timeout,
                               RequestHandler&& handler)
    {
        return requestor_.request(std::forward<C>(command), timeout,
                                  std::move(handler));
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
            void operator()(const ErrorOr<Message>&)
            {
                // Don't propagate WAMP errors, as we prefer
                // this to be a no-fail cleanup operation.
            }
        };

        if (state() == State::established)
            request(Unsubscribe{subId}, Requested{});
    }

    void sendUnsubscribe(SubscriptionId subId, TimeoutDuration timeout,
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
            request(Unsubscribe{subId}, timeout,
                    Requested{shared_from_this(), std::move(handler)});
        }
    }

    template <typename THandler>
    bool checkError(const ErrorOr<Message>& msg, THandler& handler)
    {
        if (msg == makeUnexpectedError(WampErrc::timeout))
        {
            peer_->fail();
            clear();
        }

        const bool ok = msg.has_value();
        if (!ok)
            dispatchHandler(handler, UnexpectedError(msg.error()));
        return ok;
    }

    template <typename THandler>
    bool checkReply(ErrorOr<Message>& reply, MessageKind kind,
                    THandler& handler, Error* errorPtr = nullptr)
    {
        if (kind != MessageKind::result &&
            reply == makeUnexpectedError(WampErrc::timeout))
        {
            peer_->fail();
            clear();
        }

        if (!reply.has_value())
        {
            dispatchHandler(handler, UnexpectedError(reply.error()));
            return false;
        }

        if (!checkError(reply, handler))
            return false;

        if (reply->kind() != MessageKind::error)
        {
            assert((reply->kind() == kind) && "Unexpected WAMP message type");
            return true;
        }

        Error error(PassKey{}, std::move(*reply));
        const WampErrc errc = error.errorCode();

        if (errorPtr != nullptr)
        {
            *errorPtr = std::move(error);
        }
        else if (incidentSlot_)
        {
            if (errc == WampErrc::unknown)
                report({IncidentKind::unknownErrorUri, error});
            else if (error.hasArgs())
                report({IncidentKind::errorHasPayload, error});
        }

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
        static constexpr auto errc = WampErrc::protocolViolation;
        abandonPending(errc);
        peer_->abort(Reason(errc).withHint(why));
        report({IncidentKind::commFailure, make_error_code(errc),
                std::move(why)});
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
    IoStrand strand_;
    Peer::Ptr peer_;
    Readership readership_;
    ProcedureRegistry registry_;
    Requestor requestor_;
    boost::asio::steady_timer connectionTimer_;
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
