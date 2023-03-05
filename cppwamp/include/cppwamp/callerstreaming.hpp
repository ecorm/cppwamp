/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CALLERSTREAMING_HPP
#define CPPWAMP_CALLERSTREAMING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for steaming chunks to/from callers. */
//------------------------------------------------------------------------------

#include <atomic>
#include <future>
#include <memory>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "peerdata.hpp"
#include "streaming.hpp"
#include "tagtypes.hpp"
#include "internal/caller.hpp"
#include "internal/passkey.hpp"
#include "internal/wampmessage.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains the payload of a chunk received via a progressive
    `RESULT` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerInputChunk : public Chunk<CallerInputChunk,
                                                  internal::ResultMessage>
{
public:
    /** Default constructor. */
    CallerInputChunk();

private:
    using Base = Chunk<CallerInputChunk, internal::ResultMessage>;

public:
    // Internal use only
    CallerInputChunk(internal::PassKey, internal::ResultMessage&& msg);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk to be sent via a progressive
    `CALL` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerOutputChunk : public Chunk<CallerOutputChunk,
                                                   internal::CallMessage>
{
public:
    /** Constructor. */
    explicit CallerOutputChunk(bool isFinal = false);

private:
    using Base = Chunk<CallerOutputChunk, internal::CallMessage>;

public:
    // Internal use only
    void setCallInfo(internal::PassKey, String uri);
    internal::CallMessage& callMessage(internal::PassKey, RequestId reqId);
};


//------------------------------------------------------------------------------
/** Contains signalling information used to establish a channel for streaming
    with a remote peer.
    This object is used to generate an initiating `CALL` message configured for
    progressive call results and/or invocations. */
//------------------------------------------------------------------------------
class CPPWAMP_API Invitation : public RpcLike<Invitation>
{
public:
    /** Constructor taking a stream URI and desired stream mode. */
    explicit Invitation(String uri, StreamMode mode);

    /** Obtains the desired stream mode. */
    StreamMode mode() const;

    /** Treats the initial result as a chunk instead of an RSVP. */
    Invitation& withRsvpTreatedAsChunk(bool enabled = true);

    /** Returns true if the initial result is to be treated as a chunk
        instead of an RSVP. */
    bool rsvpTreatedAsChunk() const;

private:
    using Base = RpcLike<Invitation>;

    StreamMode mode_;
    bool rsvpTreatedAsChunk_ = false;

public:
    // Internal use only
    internal::CallMessage& callMessage(internal::PassKey, RequestId reqId);
};

//------------------------------------------------------------------------------
/** Provides the interface for a caller to stream chunks of data. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerChannel
    : public std::enable_shared_from_this<CallerChannel>
{
public:
    using Ptr = std::shared_ptr<CallerChannel>;   ///< Shared pointer type
    using WeakPtr = std::weak_ptr<CallerChannel>; ///< Weak pointer type
    using InputChunk = CallerInputChunk;          ///< Input chunk type
    using OutputChunk = CallerOutputChunk;        ///< Output chunk type

    /// Enumerates the channel's possible states.
    enum class State
    {
        open,
        closed
    };

    /** Destructor which automatically cancels the stream if the
        channel is not already closed. */
    ~CallerChannel();

    /** Obtains the stream mode specified in the invitation
        associated with this channel. */
    StreamMode mode() const;

    /** Determines if an RSVP is available. */
    bool hasRsvp() const;

    /** Obtains the RSVP information returned by the callee, if any. */
    const InputChunk& rsvp() const &;

    /** Moves the RSVP information returned by the callee. */
    InputChunk&& rsvp() &&;

    /** Obtains the channel's current state. */
    State state() const;

    /** Obtains the ephemeral ID of this channel. */
    ChannelId id() const;

    /** Accesses the error reported back by the callee. */
    const Error& error() const &;

    /** Moves the error reported back by the callee. */
    Error&& error() &&;

    /** Sends a chunk to the other peer. */
    CPPWAMP_NODISCARD ErrorOrDone send(OutputChunk chunk);

    /** Thread-safe send. */
    CPPWAMP_NODISCARD std::future<ErrorOrDone> send(ThreadSafe,
                                                    OutputChunk chunk);

    /** Sends a cancellation request to the other peer and
        closes the channel. */
    ErrorOrDone cancel(CallCancelMode mode);

    /** Thread-safe cancel with mode. */
    std::future<ErrorOrDone> cancel(ThreadSafe, CallCancelMode mode);

    /** Sends a cancellation request to the other peer using the mode specified
        in the Invitation, and closes the channel. */
    ErrorOrDone cancel();

    /** Thread-safe cancel. */
    std::future<ErrorOrDone> cancel(ThreadSafe);

private:
    using CallerPtr = std::weak_ptr<internal::Caller>;
    using ChunkSlot = AnyReusableHandler<void (Ptr, ErrorOr<InputChunk>)>;

    static std::future<ErrorOrDone> futureValue(bool x);

    template <typename TErrc>
    static std::future<ErrorOrDone> futureError(TErrc errc)
    {
        std::promise<ErrorOrDone> p;
        auto f = p.get_future();
        p.set_value(makeUnexpectedError(errc));
        return f;
    }

    CallerChannel(ChannelId id, const Invitation& inv, CallerPtr caller,
                  AnyReusableHandler<void (Ptr, ErrorOr<InputChunk>)>&& onChunk,
                  AnyIoExecutor exec, AnyCompletionExecutor userExec);

    std::future<ErrorOrDone> safeCancel();

    template <typename T>
    void postChunkHandler(T&& arg)
    {
        postVia(executor_, userExecutor_, chunkSlot_, shared_from_this(),
                std::forward<T>(arg));
    }

    bool isValidModeForSending() const;

    InputChunk rsvp_;
    Error error_;
    Uri uri_;
    ChunkSlot chunkSlot_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    CallerPtr caller_;
    ChannelId id_ = nullId();
    std::atomic<State> state_;
    StreamMode mode_ = {};
    CallCancelMode cancelMode_ = CallCancelMode::unknown;
    bool hasRsvp_ = false;

public:
    // Internal use only
    static Ptr create(
        internal::PassKey,
        ChannelId id, const Invitation& inv, CallerPtr caller,
        AnyReusableHandler<void (Ptr, ErrorOr<InputChunk>)>&& onChunk,
        AnyIoExecutor exec, AnyCompletionExecutor userExec);

    void setRsvp(internal::PassKey, internal::ResultMessage&& msg);

    void postResult(internal::PassKey, internal::ResultMessage&& msg);

    void postError(internal::PassKey, internal::ErrorMessage&& msg);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/callerstreaming.ipp"
#endif

#endif // CPPWAMP_CALLERSTREAMING_HPP
