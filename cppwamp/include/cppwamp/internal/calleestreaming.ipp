/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../calleestreaming.hpp"

namespace wamp
{

//******************************************************************************
// CalleeInputChunk
//******************************************************************************

CPPWAMP_INLINE CalleeInputChunk::CalleeInputChunk() {}

CPPWAMP_INLINE CalleeInputChunk::CalleeInputChunk(
      internal::PassKey, internal::InvocationMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE StreamMode CalleeInputChunk::mode(internal::PassKey)
{
    bool wantsProgress = optionOr<bool>("receive_progress", false);

    using M = StreamMode;
    if (isFinal())
        return wantsProgress ? M::calleeToCaller : M::simpleCall;
    return wantsProgress ? M::bidirectional : M::callerToCallee;
}


//******************************************************************************
// CalleeOutputChunk
//******************************************************************************

CPPWAMP_INLINE CalleeOutputChunk::CalleeOutputChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
    )
    : Base(isFinal)
{}

CPPWAMP_INLINE internal::YieldMessage&
CalleeOutputChunk::yieldMessage(internal::PassKey, RequestId reqId)
{
    message().setRequestId(reqId);
    return message();
}


//******************************************************************************
// Stream
//******************************************************************************

CPPWAMP_INLINE Stream::Stream(String uri): Base(std::move(uri)) {}

CPPWAMP_INLINE Stream& Stream::withInvitationExpected(bool enabled)
{
    invitationExpected_ = enabled;
    return *this;
}

CPPWAMP_INLINE bool Stream::invitationExpected() const
{
    return invitationExpected_;
}

CPPWAMP_INLINE internal::RegisterMessage&
Stream::registerMessage(internal::PassKey)
{
    return message();
}


//******************************************************************************
// CalleeChannel
//******************************************************************************

CPPWAMP_INLINE CalleeChannel::~CalleeChannel()
{
    fail(threadSafe, Error{WampErrc::cancelled});
}

CPPWAMP_INLINE StreamMode CalleeChannel::mode() const {return mode_;}

/** A CalleeChannel is `inviting` upon creation, `open` upon acceptance,
    and `closed` upon sending an error or a final chunk. */
CPPWAMP_INLINE CalleeChannel::State CalleeChannel::state() const
{
    return state_.load();
}

CPPWAMP_INLINE ChannelId CalleeChannel::id() const {return id_;}

CPPWAMP_INLINE bool CalleeChannel::invitationExpected() const
{
    return invitationExpected_;
}

CPPWAMP_INLINE const CalleeInputChunk& CalleeChannel::invitation() const &
{
    static const CalleeInputChunk empty;
    return invitationExpected_ ? invitation_ : empty;
}

/** @pre `this->invitationTreatedAsChunk() == false`
    @throws error::Logic if the precondition is not met. */
CPPWAMP_INLINE CalleeInputChunk&& CalleeChannel::invitation() &&
{
    CPPWAMP_LOGIC_CHECK(
        invitationExpected_,
        "wamp::CalleeChannel::invitation: cannot move unexpected invitation");
    return std::move(invitation_);
}

/** The channel is immediately closed if the given chunk is marked as final.
    @returns
        - false if the associated Session object is destroyed or
                the streaming request no longer exists
        - true if the response was accepted for processing
        - an error code if there was a problem processing the response
    @pre `this->state() == State::inviting`
    @pre `response.isFinal() || this->mode == StreamMode::calleeToCaller ||
          this->mode == StreamMode::bidirectional`
    @post `this->state() == response.isFinal() ? State::closed : State::open`
    @throws error::Logic if the mode precondition is not met */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::accept(
    OutputChunk response,     ///< The RSVP to return back to the caller.
    ChunkSlot onChunk,        ///< Handler to use for received chunks.
    InterruptSlot onInterrupt ///< Handler to use for received interruptions.
    )
{
    CPPWAMP_LOGIC_CHECK(isValidModeFor(response),
                        "wamp::CalleeChannel::accept: invalid mode");
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

    postUnexpectedInvitationAsChunk();

    auto caller = callee_.lock();
    if (!caller)
        return false;
    return caller->sendCalleeChunk(id_, std::move(response));
}

/** @copydetails CalleeChannel::accept(OutputChunk, ChunkSlot, InterruptSlot) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::accept(
    ThreadSafe,
    OutputChunk response,     ///< The RSVP to return back to the caller.
    ChunkSlot onChunk,        ///< Handler to use for received chunks.
    InterruptSlot onInterrupt ///< Handler to use for received interruptions.
    )
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

    postUnexpectedInvitationAsChunk();

    auto caller = callee_.lock();
    if (!caller)
        return futureValue(false);
    return caller->safeSendCalleeChunk(id_, std::move(response));
}

/** This function is thread-safe.
    @returns an error code if the channel was not in the inviting state
    @pre `this->state() == State::inviting`
    @post `this->state() == State::open` */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::accept(
    ChunkSlot onChunk,        ///< Handler to use for received chunks.
    InterruptSlot onInterrupt ///< Handler to use for received interruptions.
    )
{
    State expectedState = State::inviting;
    bool ok = state_.compare_exchange_strong(expectedState, State::open);
    if (!ok)
        return makeUnexpectedError(Errc::invalidState);

    chunkHandler_ = std::move(onChunk);
    interruptHandler_ = std::move(onInterrupt);
    postUnexpectedInvitationAsChunk();

    return true;
}

/** The channel is closed if the given chunk is marked as final. */
/** @returns
        - false if the associated Session object is destroyed
        - true if the chunk was accepted for processing
        - an error code if there was a problem processing the chunk
    @pre `this->state() == State::open`
    @pre `chunk.isFinal() || this->mode == StreamMode::calleeToCaller ||
          this->mode == StreamMode::bidirectional`
    @post `this->state() == chunk.isFinal() ? State::closed : State::open`
    @throws error::Logic if the mode precondition is not met */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::send(OutputChunk chunk)
{
    State expectedState = State::open;
    auto newState = chunk.isFinal() ? State::closed : State::open;
    bool ok = state_.compare_exchange_strong(expectedState, newState);
    if (!ok)
        return makeUnexpectedError(Errc::invalidState);

    auto caller = callee_.lock();
    if (!caller)
        return false;
    return caller->sendCalleeChunk(id_, std::move(chunk));
}

/** @copydetails CalleeChannel::send(OutputChunk) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::send(ThreadSafe,
                                                            OutputChunk chunk)
{
    State expectedState = State::open;
    auto newState = chunk.isFinal() ? State::closed : State::open;
    bool ok = state_.compare_exchange_strong(expectedState, newState);
    if (!ok)
        return futureError(Errc::invalidState);
    auto callee = callee_.lock();
    if (!callee)
        return futureValue(false);
    return callee->safeSendCalleeChunk(id_, std::move(chunk));
}

/** @returns
        - false if the associated Session object is destroyed or the
                channel state is already closed
        - true if the error was accepted for processing
        - an error code if there was a problem processing the error
    @post `this->state() == State::closed` */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::fail(Error error)
{
    auto oldState = state_.exchange(State::closed);
    auto caller = callee_.lock();
    if (!caller || oldState == State::closed)
        return false;
    return caller->yield(id_, std::move(error));
}

/** @copydetails CalleeChannel::fail(Error) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::fail(ThreadSafe,
                                                             Error error)
{
    auto oldState = state_.exchange(State::closed);
    auto caller = callee_.lock();
    if (!caller || oldState == State::closed)
        return futureValue(false);
    return caller->safeYield(id_, std::move(error));
}

CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::futureValue(bool x)
{
    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(x);
    return f;
}

CPPWAMP_INLINE CalleeChannel::CalleeChannel(
    internal::InvocationMessage&& msg, const internal::StreamRegistration& reg,
    AnyIoExecutor executor, AnyCompletionExecutor userExecutor,
    CalleePtr callee)
    : invitation_({}, std::move(msg)),
      executor_(std::move(executor)),
      userExecutor_(std::move(userExecutor)),
      callee_(std::move(callee)),
      id_(invitation_.channelId()),
      state_(State::inviting),
      mode_(invitation_.mode({})),
      invitationExpected_(reg.invitationExpected)
{}

CPPWAMP_INLINE bool CalleeChannel::isValidModeFor(const OutputChunk& c) const
{
    using M = StreamMode;
    return c.isFinal() ||
           (mode_ == M::calleeToCaller) ||
           (mode_ == M::bidirectional);
}

CPPWAMP_INLINE void CalleeChannel::postUnexpectedInvitationAsChunk()
{
    if (chunkHandler_ && !invitationExpected_)
    {
        postVia(executor_, userExecutor_, chunkHandler_,
                shared_from_this(), std::move(invitation_));
        invitation_ = InputChunk{};
    }
}

CPPWAMP_INLINE CalleeChannel::Ptr
CalleeChannel::create(
    internal::PassKey, internal::InvocationMessage&& msg,
    const internal::StreamRegistration& reg, AnyIoExecutor executor,
    AnyCompletionExecutor userExecutor, CalleePtr callee)
{
    return Ptr{new CalleeChannel(std::move(msg), reg, std::move(executor),
                                 std::move(userExecutor), std::move(callee))};
}

CPPWAMP_INLINE bool CalleeChannel::hasInterruptHandler(internal::PassKey) const
{
    return interruptHandler_ != nullptr;
}

CPPWAMP_INLINE void CalleeChannel::postInvocation(
    internal::PassKey, internal::InvocationMessage&& msg)
{
    if (!chunkHandler_)
        return;
    InputChunk chunk{{}, std::move(msg)};
    postVia(executor_, userExecutor_, chunkHandler_, shared_from_this(),
            std::move(chunk));
}

CPPWAMP_INLINE void CalleeChannel::postInterrupt(
    internal::PassKey, internal::InterruptMessage&& msg)
{
    if (!interruptHandler_)
        return;
    Interruption intr{{}, std::move(msg)};
    postVia(executor_, userExecutor_, interruptHandler_, shared_from_this(),
            std::move(intr));
}

} // namespace wamp
