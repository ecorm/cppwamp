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
#include "api.hpp"
#include "rpcinfo.hpp"
#include "streaming.hpp"
#include "tagtypes.hpp"
#include "internal/passkey.hpp"
#include "internal/wampmessage.hpp"

namespace wamp
{

namespace internal
{

template <typename> class BasicCallerChannelImpl;

} // namespace internal


//------------------------------------------------------------------------------
/** Contains the stream URI, mode, options, and initial payload for opening a
    new streaming channel.
    This object is used to generate an initiating `CALL` message configured for
    progressive call results and/or invocations. */
//------------------------------------------------------------------------------
class CPPWAMP_API StreamRequest : public RpcLike<StreamRequest>
{
public:
    /** Constructor taking a stream URI and desired stream mode. */
    explicit StreamRequest(Uri uri, StreamMode mode);

    /** Obtains the desired stream mode. */
    StreamMode mode() const;

private:
    using Base = RpcLike<StreamRequest>;

    StreamMode mode_;

public:
    // Internal use only
    internal::CallMessage& callMessage(internal::PassKey, RequestId reqId);
};


//------------------------------------------------------------------------------
/** Provides the interface for a caller to stream chunks of data.
    This is a lightweight object serving as a reference-counted proxy to the
    actual channel. When the reference count reaches zero, the streaming request
    is automatically cancelled if the channel is not closed. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerChannel
{
public:
    using InputChunk = CallerInputChunk;   ///< Input chunk type
    using OutputChunk = CallerOutputChunk; ///< Output chunk type
    using State = ChannelState;            ///< Channel state type

    /** Constructs a detached channel. */
    CallerChannel();

    /** Obtains the stream mode specified in the StreamRequest associated with
        this channel. */
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

    /** Obtains the executor used to execute user-provided handlers. */
    const AnyCompletionExecutor& executor() const;

    /** Determines if this instance has shared ownership of the underlying
        channel. */
    bool attached() const;

    /** Returns true if this instance has shared ownership of the underlying
        channel. */
    explicit operator bool() const;

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
        in the StreamRequest, and closes the channel. */
    ErrorOrDone cancel();

    /** Thread-safe cancel. */
    std::future<ErrorOrDone> cancel(ThreadSafe);

    /** Releases shared ownership of the underlying channel. */
    void detach();

private:
    using Impl = internal::BasicCallerChannelImpl<CallerChannel>;

    std::shared_ptr<Impl> impl_;

public:
    // Internal use only
    CallerChannel(internal::PassKey, std::shared_ptr<Impl> impl);
};


namespace internal
{

using CallerChannelImpl = internal::BasicCallerChannelImpl<CallerChannel>;

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/callerstreaming.ipp"
#endif

#endif // CPPWAMP_CALLERSTREAMING_HPP
