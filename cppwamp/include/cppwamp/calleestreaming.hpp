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
#include "internal/passkey.hpp"
#include "internal/wampmessage.hpp"

namespace wamp
{

namespace internal
{

template <typename> class BasicCalleeChannelImpl;

} // namespace internal


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

    /** Treats the initial invocation as an invitation instead of a chunk. */
    Stream& withInvitationExpected(bool enabled = true);

    /** Returns true if the initial invocation is to be treated as an invitation
        instead of a chunk. */
    bool invitationExpected() const;

private:
    using Base = ProcedureLike<Stream>;

    bool invitationExpected_ = false;

public:
    // Internal use only
    internal::RegisterMessage& registerMessage(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Provides the interface for a callee to stream chunks of data.
    This is a lightweight object serving as a reference-counted proxy to the
    actual channel. When the reference count reaches zero, the streaming request
    is automatically terminated if the channel is not closed. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeChannel
{
public:
    using InputChunk = CalleeInputChunk;            ///< Input chunk type
    using OutputChunk = CalleeOutputChunk;          ///< Output chunk type
    using Executor = AnyIoExecutor;                 ///< Executor type
    using FallbackExecutor = AnyCompletionExecutor; ///< Fallback executor type
    using State = ChannelState;                     ///< Channel state type

    /// Type-erases the handler function for processing inbound chunks
    using ChunkSlot = AnyReusableHandler<void (CalleeChannel, InputChunk)>;

    /// Type-erases the handler function for processing interruptions
    using InterruptSlot =
        AnyReusableHandler<void (CalleeChannel, Interruption)>;

    /** Constructs a detached channel. */
    CalleeChannel();

    /** Obtains the stream mode that was established in the initial request. */
    StreamMode mode() const;

    /** Obtains the current channel state. */
    State state() const;

    /** Obtains the ephemeral ID of this channel. */
    ChannelId id() const;

    /** Determines if the Stream::withInvitationTreatedAsChunk option was set
        during stream registrtion. */
    bool invitationExpected() const;

    /** Accesses the invitation. */
    const InputChunk& invitation() const &;

    /** Moves the invitation. */
    InputChunk&& invitation() &&;

    /** Accesses the channel's executor used to post the ChunkSlot and
        InterruptSlot. */
    const Executor& executor() const;

    /** Accesses the channel's fallback executor that is bound to the ChunkSlot
        or Interrupt slot if none were already bound. */
    const FallbackExecutor& fallbackExecutor() const;

    /** Determines if this instance has shared ownership of the underlying
        channel. */
    bool attached() const;

    /** Returns true if this instance has shared ownership of the underlying
        channel. */
    explicit operator bool() const;

    /** Accepts a streaming request from another peer and sends an
        initial (or final) response. */
    CPPWAMP_NODISCARD ErrorOrDone accept(
        OutputChunk response,
        ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {});

    /** Thread-safe accept with response. */
    CPPWAMP_NODISCARD std::future<ErrorOrDone> accept(
        ThreadSafe, OutputChunk response, ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {});

    /** Accepts a streaming request from another peer, without sending an
        initial response. */
    CPPWAMP_NODISCARD ErrorOrDone accept(
        ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {});

    /** Sends a chunk to the other peer. */
    CPPWAMP_NODISCARD ErrorOrDone send(OutputChunk chunk);

    /** Thread-safe send. */
    CPPWAMP_NODISCARD std::future<ErrorOrDone>
    send(ThreadSafe, OutputChunk chunk);

    /** Sends an Error to the other peer and closes the stream. */
    ErrorOrDone fail(Error error);

    /** Thread-safe fail with error. */
    std::future<ErrorOrDone> fail(ThreadSafe, Error error);

    /** Releases shared ownership of the underlying channel. */
    void detach();

private:
    using Impl = internal::BasicCalleeChannelImpl<CalleeChannel>;

    std::shared_ptr<Impl> impl_;

public:
    // Internal use only
    CalleeChannel(internal::PassKey, std::shared_ptr<Impl> impl);
};


namespace internal
{

using CalleeChannelImpl = internal::BasicCalleeChannelImpl<CalleeChannel>;

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/calleestreaming.ipp"
#endif

#endif // CPPWAMP_CALLEESTREAMING_HPP
