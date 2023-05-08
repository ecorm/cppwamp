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
#include <memory>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "config.hpp"
#include "exceptions.hpp"
#include "rpcinfo.hpp"
#include "streaming.hpp"
#include "internal/passkey.hpp"

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
    explicit Stream(Uri uri);

    /** Treats the initial invocation as an invitation instead of a chunk. */
    Stream& withInvitationExpected(bool enabled = true);

    /** Returns true if the initial invocation is to be treated as an invitation
        instead of a chunk. */
    bool invitationExpected() const;

private:
    using Base = ProcedureLike<Stream>;

    bool invitationExpected_ = false;
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

    /** Obtains the executor used to execute user-provided handlers. */
    const AnyCompletionExecutor& executor() const;

    /** Determines if this instance has shared ownership of the underlying
        channel. */
    bool attached() const;

    /** Returns true if this instance has shared ownership of the underlying
        channel. */
    explicit operator bool() const;

    /** Accepts a streaming request from another peer and sends an
        initial (or final) response. */
    template <typename S = std::nullptr_t, typename I = std::nullptr_t>
    CPPWAMP_NODISCARD ErrorOrDone
    respond(OutputChunk response, S&& chunkSlot = nullptr,
            I&& interruptSlot = nullptr);

    /** Accepts a streaming request from another peer, without sending an
        initial response. */
    template <typename S = std::nullptr_t, typename I = std::nullptr_t>
    CPPWAMP_NODISCARD ErrorOrDone accept(S&& chunkSlot = nullptr,
                                         I&& interruptSlot = nullptr);

    /** Sends a chunk to the other peer. */
    CPPWAMP_NODISCARD ErrorOrDone send(OutputChunk chunk);

    /** Sends an Error to the other peer and closes the stream. */
    void fail(Error error);

    /** Releases shared ownership of the underlying channel. */
    void detach();

private:
    using ChunkSlot = AnyReusableHandler<void (CalleeChannel,
                                               ErrorOr<InputChunk>)>;

    using InterruptSlot =
        AnyReusableHandler<void (CalleeChannel, Interruption)>;

    using Impl = internal::BasicCalleeChannelImpl<CalleeChannel>;

    ErrorOrDone doRespond(OutputChunk&& response, ChunkSlot&& onChunk,
                          InterruptSlot&& onInterrupt);

    ErrorOrDone doAccept(ChunkSlot&& onChunk, InterruptSlot&& onInterrupt);

    std::shared_ptr<Impl> impl_;

public:
    // Internal use only
    CalleeChannel(internal::PassKey, std::shared_ptr<Impl> impl);
};


//******************************************************************************
// CalleeChannel member definitions
//******************************************************************************

/** @tparam S Callable handler with signature
              `void (CalleeChannel, InputChunk)`
    @tparam I Callable handler with signature
              `void (CalleeChannel, Interruption)`
    The channel is immediately closed if the given chunk is marked as final.
    @returns
        - false if the associated Session object is destroyed or
                the streaming request no longer exists
        - true if the response was accepted for processing
        - an error code if there was a problem processing the response
    @note This method should be called within the invocation context of the
          StreamSlot in order to losing incoming chunks or interruptions due
          to the ChunkSlot or InterruptSlot not being registered in time.
    @pre `this->state() == State::awaiting`
    @pre `response.isFinal() || this->mode == StreamMode::calleeToCaller ||
          this->mode == StreamMode::bidirectional`
    @post `this->state() == response.isFinal() ? State::closed : State::open`
    @throws error::Logic if the mode precondition is not met */
template <typename S, typename I>
ErrorOrDone CalleeChannel::respond(OutputChunk response, S&& chunkSlot,
                                   I&& interruptSlot)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::accept: "
                                    "Channel is detached");
    return doRespond(std::move(response), std::move(chunkSlot),
                     std::move(interruptSlot));
}

/** @copydetails CalleeChannel::respond(OutputChunk, S&&, I&&) */
template <typename S, typename I>
ErrorOrDone CalleeChannel::accept(S&& chunkSlot, I&& interruptSlot)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::accept: "
                                    "Channel is detached");
    return doAccept(std::move(chunkSlot), std::move(interruptSlot));
}

namespace internal
{

using CalleeChannelImpl = internal::BasicCalleeChannelImpl<CalleeChannel>;

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/calleestreaming.ipp"
#endif

#endif // CPPWAMP_CALLEESTREAMING_HPP
