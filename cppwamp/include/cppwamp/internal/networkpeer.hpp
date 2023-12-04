/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_NETWORKPEER_HPP
#define CPPWAMP_INTERNAL_NETWORKPEER_HPP

#include <string>
#include <utility>
#include "../errorcodes.hpp"
#include "message.hpp"
#include "peer.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Provides facilities for communicating with a remote WAMP peer
//------------------------------------------------------------------------------
class NetworkPeer : public Peer
{
public:
    using Ptr = std::shared_ptr<NetworkPeer>;

    explicit NetworkPeer(bool isRouter) : Base(isRouter) {}

    ~NetworkPeer() override
    {
        if (transport_)
            transport_->close();
    }

    std::error_code monitor()
    {
        return (transport_ == nullptr) ? std::error_code{}
                                       : transport_->monitor();
    }

    ErrorOrDone sendMessage(Message& msg)
    {
        assert(msg.kind() != MessageKind::none);

        MessageBuffer buffer;
        msg.trim();
        codec_.encode(msg.fields(), buffer);
        if (buffer.size() > sendLimit_)
            return makeUnexpectedError(WampErrc::payloadSizeExceeded);

        traceTx(msg);
        transport_->send(std::move(buffer));
        return true;
    }

    ErrorOrDone abort(Abort reason) override
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            return makeUnexpectedError(MiscErrc::invalidState);

        if (!readyToAbort())
        {
            disconnect();
            return makeUnexpectedError(MiscErrc::invalidState);
        }

        const auto& msg = reason.message({});
        MessageBuffer buffer;
        codec_.encode(msg.fields(), buffer);

        const bool fits = buffer.size() <= sendLimit_;
        if (!fits)
        {
            reason.options().clear();
            reason.withHint("(snipped)");
            buffer.clear();
            codec_.encode(msg.fields(), buffer);
        }

        traceTx(msg);
        setState(State::failed);

        auto self = std::dynamic_pointer_cast<NetworkPeer>(shared_from_this());
        transport_->abort(
            std::move(buffer),
            [this, self](std::error_code ec)
            {
                transport_->close();
                transport_.reset();
                if (ec)
                    fail("Transport shutdown failure", ec);
            });

        if (!fits)
            return makeUnexpectedError(WampErrc::payloadSizeExceeded);
        return true;
    }

    ErrorOrDone send(Goodbye&& c) override           {return sendCommand(c);}
    ErrorOrDone send(Hello&& c) override             {return sendCommand(c);}
    ErrorOrDone send(Welcome&& c) override           {return sendCommand(c);}
    ErrorOrDone send(Authentication&& c) override    {return sendCommand(c);}
    ErrorOrDone send(Challenge&& c) override         {return sendCommand(c);}
    ErrorOrDone send(Topic&& c) override             {return sendCommand(c);}
    ErrorOrDone send(Pub&& c) override               {return sendCommand(c);}
    ErrorOrDone send(Event&& c) override             {return sendCommand(c);}
    ErrorOrDone send(Subscribed&& c) override        {return sendCommand(c);}
    ErrorOrDone send(Unsubscribe&& c) override       {return sendCommand(c);}
    ErrorOrDone send(Unsubscribed&& c) override      {return sendCommand(c);}
    ErrorOrDone send(Published&& c) override         {return sendCommand(c);}
    ErrorOrDone send(Procedure&& c) override         {return sendCommand(c);}
    ErrorOrDone send(Rpc&& c) override               {return sendCommand(c);}
    ErrorOrDone send(Result&& c) override            {return sendCommand(c);}
    ErrorOrDone send(Invocation&& c) override        {return sendCommand(c);}
    ErrorOrDone send(CallCancellation&& c) override  {return sendCommand(c);}
    ErrorOrDone send(Interruption&& c) override      {return sendCommand(c);}
    ErrorOrDone send(Registered&& c) override        {return sendCommand(c);}
    ErrorOrDone send(Unregister&& c) override        {return sendCommand(c);}
    ErrorOrDone send(Unregistered&& c) override      {return sendCommand(c);}
    ErrorOrDone send(Stream&& c) override            {return sendCommand(c);}
    ErrorOrDone send(StreamRequest&& c) override     {return sendCommand(c);}
    ErrorOrDone send(CalleeOutputChunk&& c) override {return sendCommand(c);}
    ErrorOrDone send(CallerOutputChunk&& c) override {return sendCommand(c);}

    NetworkPeer(const NetworkPeer&) = delete;
    NetworkPeer(NetworkPeer&&) = delete;
    NetworkPeer& operator=(const NetworkPeer&) = delete;
    NetworkPeer& operator=(NetworkPeer&&) = delete;

private:
    using Base = Peer;

    ErrorOrDone send(Error&& error) override
    {
        auto done = sendCommand(error);
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            error.snip({});
            sendCommand(error);
        }
        return done;
    }
    
    void onDirectConnect(IoStrand, any) override
    {
        assert(false);
    }

    void onConnect(Transporting::Ptr t, AnyBufferCodec c) override
    {
        transport_ = std::move(t);
        ++transportId_;
        codec_ = std::move(c);
        sendLimit_ = transport_->info().sendLimit();

        if (transport_->state() == TransportState::ready)
        {
            auto id = transportId_;
            const std::weak_ptr<NetworkPeer> weakSelf =
                std::static_pointer_cast<NetworkPeer>(shared_from_this());
            transport_->start(
                [weakSelf, id](const ErrorOr<MessageBuffer>& buffer)
                {
                    auto self = weakSelf.lock();
                    if (self)
                        self->onTransportRx(buffer, id);
                },
                [weakSelf, id](std::error_code ec)
                {
                    auto self = weakSelf.lock();
                    if (self)
                        self->onTransportTxError(ec, id);
                });
        }
    }

    void onClose() override {/* Nothing to do*/}

    void onDisconnect(State) override
    {
        if (transport_)
        {
            transport_->close();
            transport_.reset();
        }
    }

    void onDisconnectGracefully(State, DisconnectHandler handler) override
    {
        struct ShutDown
        {
            DisconnectHandler handler;
            Ptr self;

            void operator()(std::error_code ec)
            {
                auto& me = *self;
                me.transport_->close();
                me.transport_.reset();
                if (ec)
                {
                    me.fail("Transport shutdown failure", ec);
                    handler(makeUnexpected(ec));
                }
                else
                {
                    me.setState(State::disconnected);
                    handler(true);
                }
            }
        };

        if (transport_)
        {
            auto self =
                std::dynamic_pointer_cast<NetworkPeer>(shared_from_this());
            transport_->shutdown({}, ShutDown{std::move(handler),
                                              std::move(self)});
        }
        else
        {
            handler(false);
        }
    }

    void onTransportRx(const ErrorOr<MessageBuffer>& buffer,
                       std::size_t transportId)
    {
        // Ignore queued events from former transport instances
        if (!transport_ || (transportId != transportId_))
            return;

        // Ignore transport cancellation errors when disconnecting.
        if (buffer.has_value())
            onTransportRxData(*buffer);
        else if (buffer.error() == TransportErrc::disconnected)
            onRemoteDisconnect();
        else if (state() != State::disconnected)
            fail("Transport receive failure", buffer.error());
    }

    void onTransportTxError(std::error_code ec,
                            std::size_t transportId)
    {
        // Ignore queued events from former transport instances
        if (!transport_ || (transportId != transportId_))
            return;

        // Ignore transport cancellation errors when disconnecting.
        if (state() != State::disconnected)
            fail("Transport send failure", ec);
    }

    template <typename C>
    ErrorOrDone sendCommand(C&& command)
    {
        return sendMessage(command.message({}));
    }

    void onTransportRxData(const MessageBuffer& buffer)
    {
        // Ignore messages that may have been already posted by the transport
        // when disconnection occurred.
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            return;

        Variant v;
        auto ec = codec_.decode(buffer, v);
        if (ec)
            return failProtocol("Error deserializing received WAMP message: " +
                                detailedErrorCodeString(ec));

        if (!v.is<Array>())
            return failProtocol("Received WAMP message is not an array");

        auto& fields = v.as<Array>();
        traceRx(fields);

        auto msg = Message::parse(std::move(fields));
        if (!msg)
        {
            return failProtocol("Received WAMP message has invalid type number "
                                "or field schema");
        }

        const bool isValidForRole = isRouter() ? msg->traits().isRouterRx
                                               : msg->traits().isClientRx;
        if (!isValidForRole)
        {
            return failProtocol("Role does not support receiving " +
                                std::string(msg->name()) + " messages");
        }

        if (!msg->traits().isValidForState(s))
        {
            // Crossbar can spuriously send ERROR messages between a session
            // closing and reopening. Allow ERROR messages while not
            // established so that an Incident may be emitted.
            // https://github.com/crossbario/crossbar/issues/2068
            if (msg->kind() != MessageKind::error)
            {
                return failProtocol(
                    std::string(msg->name()) + " messages are invalid during " +
                    stateLabel(s) + " session state");
            }
        }

        onMessage(*msg);
    }

    void onMessage(Message& msg)
    {
        switch (msg.kind())
        {
        case MessageKind::hello:        return onHello(msg);
        case MessageKind::welcome:      return onWelcome(msg);
        case MessageKind::abort:        return onAbort(msg);
        case MessageKind::challenge:    return onChallenge(msg);
        case MessageKind::authenticate: return onAuthenticate(msg);
        case MessageKind::goodbye:      return onGoodbye(msg);
        default: break;
        }

        // Discard new requests if we're shutting down.
        auto s = state();
        bool isShuttingDown = s == State::shuttingDown ||
                              s == State::disconnecting;
        if (isShuttingDown && !msg.isReply())
            return;

        notifyMessage(msg);
    }

    void onHello(Message& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        listener().onPeerHello(Hello{{}, std::move(msg)});
    }

    void onWelcome(Message& msg)
    {
        auto s = state();
        assert(s == State::establishing || s == State::authenticating);
        setState(State::established);
        notifyMessage(msg);
    }

    void onAbort(Message& msg)
    {
        auto s = state();
        const bool wasJoining = s == State::establishing ||
                                s == State::authenticating;
        Abort reason{PassKey{}, std::move(msg)};
        setState(wasJoining ? State::closed : State::failed);
        listener().onPeerAbort(std::move(reason), wasJoining);
    }

    void onChallenge(Message& msg)
    {
        assert(state() == State::establishing);
        setState(State::authenticating);
        listener().onPeerChallenge(Challenge{{}, std::move(msg)});
    }

    void onAuthenticate(Message& msg)
    {
        assert(state() == State::authenticating);
        listener().onPeerAuthenticate(Authentication{{}, std::move(msg)});
    }

    void onGoodbye(Message& msg)
    {
        Goodbye reason{{}, std::move(msg)};
        const bool isShuttingDown = state() == State::shuttingDown;
        listener().onPeerGoodbye(std::move(reason), isShuttingDown);
    }

    void notifyMessage(Message& msg)
    {
        listener().onPeerMessage(std::move(msg));
    }

    void onRemoteDisconnect()
    {
        auto s = state();
        if (s == State::disconnected || s == State::failed)
            return;

        disconnect();
        listener().onPeerDisconnect();
    }

    void fail(std::string why, std::error_code ec)
    {
        Base::fail();
        listener().onPeerFailure(ec, false, std::move(why));
    }

    void failProtocol(std::string why)
    {
        auto ec = make_error_code(WampErrc::protocolViolation);
        if (readyToAbort())
        {
            auto reason = Abort(ec).withHint(why);
            listener().onPeerFailure(ec, true, std::move(why));
            abort(std::move(reason));
        }
        else
        {
            fail(std::move(why), ec);
        }
    }

    bool readyToAbort() const
    {
        auto s = state();
        return s == State::establishing ||
               s == State::authenticating ||
               s == State::established;
    }

    Transporting::Ptr transport_;
    AnyBufferCodec codec_;
    std::size_t sendLimit_ = 0;
    std::size_t transportId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_NETWORKPEER_HPP
