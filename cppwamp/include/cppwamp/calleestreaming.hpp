/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CALLEESTREAMING_HPP
#define CPPWAMP_CALLEESTREAMING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for steaming from/to callees. */
//------------------------------------------------------------------------------

#include <atomic>
#include <future>
#include <memory>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "config.hpp"
#include "peerdata.hpp"
#include "streaming.hpp"
#include "tagtypes.hpp"
#include "internal/callee.hpp"
#include "internal/passkey.hpp"
#include "internal/wampmessage.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains the payload of a chunk received via a progressive
    `INVOCATION` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeInputChunk : public Chunk<CalleeInputChunk,
                                                  internal::InvocationMessage>
{
public:
    /** Default constructor. */
    CalleeInputChunk();

private:
    using Base = Chunk<CalleeInputChunk, internal::InvocationMessage>;

public:
    // Internal use only
    CalleeInputChunk(internal::PassKey, internal::InvocationMessage&& msg);
    StreamMode mode(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk to be sent via a progressive
    `YIELD` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeOutputChunk : public Chunk<CalleeOutputChunk,
                                                   internal::YieldMessage>
{
public:
    /** Constructor. */
    explicit CalleeOutputChunk(bool isFinal = false);

private:
    using Base = Chunk<CalleeOutputChunk, internal::YieldMessage>;

public:
    // Internal use only
    internal::YieldMessage& yieldMessage(internal::PassKey, RequestId reqId);
};


//------------------------------------------------------------------------------
/** Contains the URI and options associated with a streaming endpoint.
    This object is used to generate a `REGISTER` message intended for use
    with progressive call results/invocations. */
//------------------------------------------------------------------------------
class CPPWAMP_API Stream : public ProcedureLike<Stream>
{
public:
    /** Constructor taking an URI with which to associate this
        streaming endpoint. */
    explicit Stream(String uri);

    /** Treats the initial invocation as a chunk instead of an invitation. */
    Stream& disableInvitation(bool disabled = true);

    /** Returns true if the initial invocation is to be treated as a chunk
        instead of an invitation. */
    bool invitationDisabled() const;

private:
    using Base = ProcedureLike<Stream>;

    bool invitationDisabled_ = false;

public:
    // Internal use only
    internal::RegisterMessage& registerMessage(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Provides the interface for a caller to stream chunks of data. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeChannel
    : public std::enable_shared_from_this<CalleeChannel>
{
public:
    using Ptr = std::shared_ptr<CalleeChannel>;
    using InputChunk = CalleeInputChunk;
    using OutputChunk = CalleeOutputChunk;
    using ChunkSlot =
        AnyReusableHandler<void (CalleeChannel::Ptr, CalleeInputChunk)>;
    using InterruptSlot =
        AnyReusableHandler<void (CalleeChannel::Ptr, Interruption)>;

    enum class State
    {
        inviting,
        open,
        closed
    };

    ~CalleeChannel();

    /** Obtains the stream mode that was established from the invitation. */
    StreamMode mode() const;

    /** Obtains the current channel state. */
    State state() const;

    /** Obtains the ephemeral ID of this channel. */
    ChannelId id() const;

    /** Determines if the ignore invitation option was set during stream
        registrtion. */
    bool invitationDisabled() const;

    /** Accesses the invitation. */
    const InputChunk& invitation() const &;

    /** Moves the invitation. */
    InputChunk&& invitation() &&;

    /** Accepts a streaming invitation from another peer and sends an
        initial (or final) response. */
    CPPWAMP_NODISCARD ErrorOrDone accept(
        OutputChunk response,
        ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {});

    /** Thread-safe accept with response. */
    CPPWAMP_NODISCARD std::future<ErrorOrDone> accept(
        ThreadSafe, OutputChunk response, ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {});

    /** Accepts a streaming invitation from another peer, without sending an
        initial response. */
    CPPWAMP_NODISCARD ErrorOrDone accept(
        ChunkSlot onChunk,
        InterruptSlot onInterrupt = {});

    /** Sends a chunk to the other peer. */
    CPPWAMP_NODISCARD ErrorOrDone send(OutputChunk chunk);

    /** Thread-safe send. */
    CPPWAMP_NODISCARD std::future<ErrorOrDone>
    send(ThreadSafe, OutputChunk chunk);

    /** Sends an Error to the other peer and closes the stream. */
    ErrorOrDone close(Error error);

    /** Thread-safe close with error. */
    std::future<ErrorOrDone> close(ThreadSafe, Error error);

private:
    using CalleePtr = std::weak_ptr<internal::Callee>;

    static std::future<ErrorOrDone> futureValue(bool x);

    template <typename TErrc>
    static std::future<ErrorOrDone> futureError(TErrc errc)
    {
        std::promise<ErrorOrDone> p;
        auto f = p.get_future();
        p.set_value(makeUnexpectedError(errc));
        return f;
    }

    CalleeChannel(internal::InvocationMessage&& msg, bool invitationDisabled,
                  AnyIoExecutor executor, AnyCompletionExecutor userExecutor,
                  CalleePtr callee);

    bool isValidModeFor(const OutputChunk& c) const;

    std::future<ErrorOrDone> safeSendChunk(OutputChunk&& chunk);

    void postInvitationAsChunkIfIgnored();

    InputChunk invitation_;
    AnyReusableHandler<void (Ptr, InputChunk)> chunkHandler_;
    AnyReusableHandler<void (Ptr, Interruption)> interruptHandler_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    CalleePtr callee_;
    ChannelId id_ = nullId();
    std::atomic<State> state_;
    StreamMode mode_ = {};
    bool invitationDisabled_ = false;

public:
    // Internal use only
    static Ptr create(internal::PassKey, internal::InvocationMessage&& msg,
                      bool invitationDisabled, AnyIoExecutor executor,
                      AnyCompletionExecutor userExecutor, CalleePtr callee);

    bool hasInterruptHandler(internal::PassKey) const;

    void onInvocation(internal::PassKey, internal::InvocationMessage&& msg);

    void onInterrupt(internal::PassKey, internal::InterruptMessage&& msg);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/calleestreaming.ipp"
#endif

#endif // CPPWAMP_CALLEESTREAMING_HPP
