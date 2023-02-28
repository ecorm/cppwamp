/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_STREAMING_HPP
#define CPPWAMP_STREAMING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for steaming chunks to/from peers. */
//------------------------------------------------------------------------------

#include <atomic>
#include <future>
#include <memory>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "peerdata.hpp"
#include "tagtypes.hpp"
#include "internal/callee.hpp"
#include "internal/caller.hpp"
#include "internal/passkey.hpp"
#include "internal/wampmessage.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/// Ephemeral ID associated with a streaming channel
//------------------------------------------------------------------------------
using ChannelId = RequestId;

//------------------------------------------------------------------------------
enum class Direction
{
    calleeToCaller,
    callerToCallee,
    bidirectional
};

//------------------------------------------------------------------------------
/** Contains the payload arguments of a chunk streamed via a progressive
    `CALL` message.
    See [Progressive Calls in the WAMP Specification]
    (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-calls) */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerChunk : public Payload<CallerChunk,
                                               internal::CallMessage>
{
public:
    /** Constructor. */
    explicit CallerChunk(bool isFinal = false);

    /** Indicates if the chunk is the final one. */
    bool isFinal() const;

private:
    using Base = Payload<CallerChunk, internal::CallMessage>;

    /** Disable the setting of options. */
    using Base::withOption;

    /** Disable the setting of options. */
    using Base::withOptions;

    bool isFinal_ = false;

public:
    // Internal use only
    CallerChunk(internal::PassKey, internal::CallMessage&& msg);
    void setCallInfo(internal::PassKey, String uri);
    internal::CallMessage& callMessage(internal::PassKey, RequestId reqId);
};


//------------------------------------------------------------------------------
/** Contains the payload arguments of a chunk streamed via a progressive
    `RESULT` message.
    See [Progressive Call Results in the WAMP Specification]
    (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-call-results) */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeChunk : public Payload<CalleeChunk,
                                               internal::ResultMessage>
{
public:
    /** Constructor. */
    explicit CalleeChunk(bool isFinal = false);

    /** Indicates if the chunk is the final one. */
    bool isFinal() const;

private:
    using Base = Payload<CalleeChunk, internal::ResultMessage>;

    /** Disable the setting of options. */
    using Base::withOption;

    /** Disable the setting of options. */
    using Base::withOptions;

    bool isFinal_ = false;

public:
    // Internal use only
    CalleeChunk(internal::PassKey, internal::ResultMessage&& msg);
    RequestId requestId(internal::PassKey) const;
    internal::ResultMessage& resultMessage(internal::PassKey, RequestId reqId);
    internal::YieldMessage& yieldMessage(internal::PassKey, RequestId reqId);
};


//------------------------------------------------------------------------------
/** Contains the URI and options associated with a streaming endpoint.
    This object is used to generate a `REGISTER` message intended for use
    with [Progressive Calls][1] and/or [Progressive Call Results][2].
    [1]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-calls)
    [2]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-call-results) */
//------------------------------------------------------------------------------
class CPPWAMP_API Stream : public Procedure
{
public:
    /** Constructor. */
    explicit Stream(
        String uri ///< The URI with which to associate this streaming endpoint.
        )
        : Base(std::move(uri))
    {}

private:
    using Base = Procedure;

public:
    // Internal use only
    internal::RegisterMessage& registerMessage(internal::PassKey)
    {
        return message();
    }
};


//------------------------------------------------------------------------------
/** Contains signalling information used to establish a channel for streaming
    with a remote peer.
    This object is used to generate an initiating `CALL` message configured for
    [Progressive Calls][1] and/or [Progressive Call Results][2].
    [1]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-calls)
    [2]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-call-results) */
//------------------------------------------------------------------------------
class CPPWAMP_API Invitation : public Rpc
{
public:
    /** Constructor. */
    explicit Invitation(
        String uri, ///< The URI with which to associate this invitation.
        Direction streamDir ///< The desired stream direction(s)
        )
        : Base(std::move(uri)),
          direction_(streamDir)
    {
        using D = Direction;
        if (streamDir == D::calleeToCaller || streamDir == D::bidirectional)
            withProgressiveResults();
        if (streamDir == D::callerToCallee || streamDir == D::bidirectional)
            withProgress();
    }

    Direction streamDirection() const {return direction_;}

private:
    using Base = Rpc;

    using Base::captureError;
    using Base::withProgressiveResults;
    using Base::progressiveResultsAreEnabled;
    using Base::withProgress;
    using Base::isProgress;

    Direction direction_;

public:
    // Internal use only
    internal::CallMessage& callMessage(internal::PassKey)
    {
        return message();
    }
};

//------------------------------------------------------------------------------
/** Contains a response to a streaming invitation.
    This object is used to generate an optional `RESULT` or `ERROR` message
    upon receipt of the signalling `CALL`.
    See [Progressive Calls][1] and/or [Progressive Call Results][2].
    [1]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-calls)
    [2]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-call-results) */
//------------------------------------------------------------------------------
class CPPWAMP_API Rsvp
{
public:
    Rsvp() : Rsvp(null) {}

    Rsvp(Null) {}

    Rsvp(CalleeChunk response)
        : message_(std::move(response.resultMessage({}, nullId())))
    {}

    Rsvp(Error error)
        : message_(std::move(error.errorMessage({}, internal::WampMsgType::call,
                                                nullId())))
    {}

private:
    internal::WampMessage message_;

public:
        // Internal use only
};

//------------------------------------------------------------------------------
/** Provides the interface for a caller to stream chunks of data. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerChannel
    : public std::enable_shared_from_this<CallerChannel>
{
public:
    using Ptr = std::shared_ptr<CallerChannel>;

    enum class State
    {
        open,
        closed
    };

    ~CallerChannel()
    {
        auto oldState = state_.exchange(State::closed);
        if (oldState != State::closed)
            safeCancel();
    }

    /** Obtains the RSVP information returned by the callee, if any. */
    const Result& rsvp() const & {return rsvp_;}

    /** Moves the RSVP information returned by the callee. */
    Result&& rsvp() && {return std::move(rsvp_);}

    /** Obtains the channel's current state.
        This channel is open upon creation and closed upon sending
        a final chunkor cancelling. */
    State state() const {return state_.load();}

    /** Obtains the ephemeral ID of this channel. */
    ChannelId id() const {return id_;}

    /** Accesses the error reported back by the callee. */
    const Error& error() const & {return error_;}

    /** Moves the error reported back by the callee. */
    Error&& error() && {return std::move(error_);}

    /** Sends a chunk to the other peer. */
    /** The channel is closed if the given chunk is marked as final.
        @returns
            - false if the associated Session object is destroyed or
                    the streaming request no longer exists
            - true if the chunk was accepted for processing
            - an error code if there was a problem processing the chunk
        @pre `this->state() == State::open`
        @post `chunk.isFinal() ? State::closed : State::open` */
    ErrorOrDone send(CallerChunk chunk)
    {
        State expectedState = State::open;
        auto newState = chunk.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return makeUnexpectedError(Errc::invalidState);

        auto caller = caller_.lock();
        if (!caller)
            return false;
        chunk.setCallInfo({}, uri_);
        return caller->sendCallerChunk(id_, std::move(chunk));
    }

    /** Thread-safe send. */
    std::future<ErrorOrDone> send(ThreadSafe, CallerChunk chunk)
    {
        State expectedState = State::open;
        auto newState = chunk.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return futureError(Errc::invalidState);

        auto caller = caller_.lock();
        if (!caller)
            return futureValue(false);
        chunk.setCallInfo({}, uri_);
        return caller->safeSendCallerChunk(id_, std::move(chunk));
    }

    /** Sends a cancellation request to the other peer and
        closes the channel. */
    /** @returns
            - false if the associated Session object is destroyed or
                    the streaming request no longer exists
            - true if the cancellation was accepted for processing
            - an error code if there was a problem processing the cancellation
        @pre `this->state() == State::open`
        @post `this->state() == State::closed` */
    ErrorOrDone cancel(CallCancelMode mode)
    {
        State expectedState = State::open;
        bool ok = state_.compare_exchange_strong(expectedState, State::closed);
        if (!ok)
            return makeUnexpectedError(Errc::invalidState);

        auto caller = caller_.lock();
        if (!caller)
            return false;
        return caller->cancelCall(id_, mode);
    }

    /** Thread-safe cancel with mode. */
    /** @copydetails cancel(CallCancelMode) */
    std::future<ErrorOrDone> cancel(ThreadSafe, CallCancelMode mode)
    {
        State expectedState = State::open;
        bool ok = state_.compare_exchange_strong(expectedState, State::closed);
        if (!ok)
            return futureError(Errc::invalidState);

        auto caller = caller_.lock();
        if (!caller)
        {
            std::promise<ErrorOrDone> p;
            p.set_value(false);
            return p.get_future();
        }
        return caller->safeCancelCall(id_, mode);
    }

    /** Sends a cancellation request to the other peer using the mode specified
        in the Invitation, and closes the channel. */
    /** @copydetails cancel(CallCancelMode) */
    ErrorOrDone cancel() {return cancel(cancelMode_);}

    /** Thread-safe cancel. */
    /** @copydetails cancel(CallCancelMode) */
    std::future<ErrorOrDone> cancel(ThreadSafe)
    {
        return cancel(threadSafe, cancelMode_);
    }

private:
    using CallerPtr = std::weak_ptr<internal::Caller>;

    static std::future<ErrorOrDone> futureValue(bool x)
    {
        std::promise<ErrorOrDone> p;
        auto f = p.get_future();
        p.set_value(x);
        return f;
    }

    template <typename TErrc>
    static std::future<ErrorOrDone> futureError(TErrc errc)
    {
        std::promise<ErrorOrDone> p;
        auto f = p.get_future();
        p.set_value(makeUnexpectedError(errc));
        return f;
    }

    std::future<ErrorOrDone> safeCancel()
    {
        auto caller = caller_.lock();
        if (!caller)
        {
            std::promise<ErrorOrDone> p;
            p.set_value(false);
            return p.get_future();
        }
        return caller->safeCancelStream(id_);
    }

    template <typename T>
    void dispatchChunkHandler(T&& arg)
    {
        dispatchVia(executor_, userExecutor_, chunkHandler_,
                    shared_from_this(), std::forward<T>(arg));
    }

    Result rsvp_;
    Error error_;
    Uri uri_;
    AnyReusableHandler<void (Ptr, ErrorOr<CalleeChunk>)> chunkHandler_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    CallerPtr caller_;
    ChannelId id_ = nullId();
    CallCancelMode cancelMode_ = CallCancelMode::unknown;
    std::atomic<State> state_;
    bool wasRsvped_ = false;

public:
    // Internal use only
    CallerChannel(
        internal::PassKey, CallerPtr caller, const Invitation& inv,
        AnyIoExecutor exec, AnyCompletionExecutor userExec,
        AnyReusableHandler<void (Ptr, ErrorOr<CalleeChunk>)> chunkHandler)
        : uri_(inv.uri()),
          chunkHandler_(std::move(chunkHandler)),
          executor_(std::move(exec)),
          userExecutor_(std::move(userExec)),
          caller_(std::move(caller)),
          cancelMode_(inv.cancelMode()),
          state_(State::open)
    {}

    bool wasRsvped(internal::PassKey) const {return wasRsvped_;}

    void setChannelId(internal::PassKey, ChannelId id) {id_ = id;}

    void onRsvp(internal::PassKey, Result&& result)
    {
        rsvp_ = std::move(result);
        wasRsvped_ = true;
    }

    void onReply(internal::PassKey, ErrorOr<internal::WampMessage>&& reply)
    {
        if (!chunkHandler_)
            return;

        if (!reply)
        {
            chunkHandler_(shared_from_this(), UnexpectedError{reply.error()});
        }
        else if (reply->type() == internal::WampMsgType::error)
        {
            auto& msg = internal::messageCast<internal::ErrorMessage>(*reply);
            error_ = Error{{}, std::move(msg)};
            auto errc = error_.errorCode();
            dispatchChunkHandler(makeUnexpectedError(errc));
        }
        else
        {
            auto& msg = internal::messageCast<internal::ResultMessage>(*reply);
            CalleeChunk chunk{{}, std::move(msg)};
            dispatchChunkHandler(std::move(chunk));
        }
    }
};

//------------------------------------------------------------------------------
/** Provides the interface for a caller to stream chunks of data. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeChannel
{
public:
    using Ptr = std::shared_ptr<CalleeChannel>;
    using Chunk = CalleeChunk;
    using ChunkSlot = AnyReusableHandler<void (CalleeChannel::Ptr,
                                               CallerChunk)>;
    using InterruptSlot = AnyReusableHandler<void (CalleeChannel::Ptr,
                                                   Interruption)>;

    enum class State
    {
        inviting,
        open,
        closed
    };

    ~CalleeChannel()
    {
        auto oldState = state_.exchange(State::closed);
        if (oldState != State::closed)
            safeSendChunk(Chunk{true});
    }

    /** Obtains the current channel state.
        A channel is inviting upon creation, open upon acceptance, and closed
        upon sending an error or a final chunk. */
    State state() const {return state_.load();}

    /** Obtains the ephemeral ID of this channel. */
    ChannelId id() const {return id_;}

    /** Accepts a streaming invitation from another peer and sends an
        initial response.
        The channel is immediately closed if the given chunk is marked as
        final. */
    /** @returns
            - false if the associated Session object is destroyed or
                    the streaming request no longer exists
            - true if the response was accepted for processing
            - an error code if there was a problem processing the response
        @pre `this->state() == State::inviting`
        @post `this->state() == response.isFinal() ? State::closed : State::open` */
    ErrorOrDone accept(
        Chunk response,
        ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {})
    {
        State expectedState = State::inviting;
        auto newState = response.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return makeUnexpectedError(Errc::invalidState);

        if (!response.isFinal())
        {
            chunkHandler_ = std::move(onChunk);
            interruptHandler_ = std::move(onInterrupt);
        }
        auto caller = callee_.lock();
        if (!caller)
            return false;
        return caller->sendCalleeChunk(id_, std::move(response));
    }

    /** Thread-safe accept with response. */
    std::future<ErrorOrDone> accept(
        ThreadSafe, Chunk response, ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {})
    {
        State expectedState = State::inviting;
        auto newState = response.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return futureError(Errc::invalidState);

        if (!response.isFinal())
        {
            chunkHandler_ = std::move(onChunk);
            interruptHandler_ = std::move(onInterrupt);
        }
        auto caller = callee_.lock();
        if (!caller)
            return futureValue(false);
        return caller->safeSendCalleeChunk(id_, std::move(response));
    }

    /** Accepts a streaming invitation from another peer, without sending an
        initial response. */
    /** This function is thread-safe.
        @returns an error code if the channel was not in the inviting state
        @pre `this->state() == State::inviting`
        @post `this->state() == State::open` */
    ErrorOrDone accept(
        ChunkSlot onChunk,
        InterruptSlot onInterrupt = {})
    {
        State expectedState = State::inviting;
        bool ok = state_.compare_exchange_strong(expectedState, State::open);
        if (!ok)
            return makeUnexpectedError(Errc::invalidState);

        chunkHandler_ = std::move(onChunk);
        interruptHandler_ = std::move(onInterrupt);
        state_.store(State::open);
        return true;
    }

    /** Sends a chunk to the other peer.
        The channel is closed if the given chunk is marked as final. */
    /** @returns
            - false if the associated Session object is destroyed
            - true if the chunk was accepted for processing
            - an error code if there was a problem processing the chunk
        @pre `this->state() == State::open`
        @post `this->state() == chunk.isFinal() ? State::closed : State::open` */
    ErrorOrDone send(Chunk chunk)
    {
        State expectedState = State::inviting;
        auto newState = chunk.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return makeUnexpectedError(Errc::invalidState);

        auto caller = callee_.lock();
        if (!caller)
            return false;
        return caller->sendCalleeChunk(id_, std::move(chunk));
    }

    /** Thread-safe send. */
    std::future<ErrorOrDone> send(ThreadSafe, Chunk chunk)
    {
        State expectedState = State::inviting;
        auto newState = chunk.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return futureError(Errc::invalidState);
        return safeSendChunk(std::move(chunk));
    }

    /** Sends an Error to the other peer and closes the stream. */
    /** @returns
            - false if the associated Session object is destroyed or the
                    channel state is already closed
            - true if the error was accepted for processing
            - an error code if there was a problem processing the error
        @pre `this->isOpen() == true`
        @post `this->state() == State::closed`
        @throws error::Logic if the preconditions were not met. */
    ErrorOrDone close(Error error)
    {
        auto oldState = state_.exchange(State::closed);
        auto caller = callee_.lock();
        if (!caller || oldState == State::closed)
            return false;
        return caller->yield(id_, std::move(error));
    }

    /** Thread-safe close with error. */
    std::future<ErrorOrDone> close(ThreadSafe, Error error)
    {
        auto oldState = state_.exchange(State::closed);
        auto caller = callee_.lock();
        if (!caller || oldState == State::closed)
            return futureValue(false);
        return caller->safeYield(id_, std::move(error));
    }

private:
    using CalleePtr = std::weak_ptr<internal::Callee>;

    static std::future<ErrorOrDone> futureValue(bool x)
    {
        std::promise<ErrorOrDone> p;
        auto f = p.get_future();
        p.set_value(x);
        return f;
    }

    template <typename TErrc>
    static std::future<ErrorOrDone> futureError(TErrc errc)
    {
        std::promise<ErrorOrDone> p;
        auto f = p.get_future();
        p.set_value(makeUnexpectedError(errc));
        return f;
    }

    CalleeChannel(const CalleeChannel& rhs)
        : chunkHandler_(rhs.chunkHandler_),
          interruptHandler_(rhs.interruptHandler_),
          callee_(rhs.callee_),
          id_(rhs.id_),
          state_(State::inviting)
    {}

    std::future<ErrorOrDone> safeSendChunk(Chunk&& chunk)
    {
        auto caller = callee_.lock();
        if (!caller)
            return futureValue(false);
        return caller->safeSendCalleeChunk(id_, std::move(chunk));
    }

    AnyReusableHandler<void (Ptr, CallerChunk)> chunkHandler_;
    AnyReusableHandler<void (Ptr, Interruption)> interruptHandler_;
    CalleePtr callee_;
    ChannelId id_ = nullId();
    std::atomic<State> state_;

public:
    // Internal use only
    CalleeChannel(internal::PassKey, CalleePtr callee)
        : callee_(std::move(callee)),
          state_(State::inviting)
    {}
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/streaming.ipp"
#endif

#endif // CPPWAMP_STREAMING_HPP
