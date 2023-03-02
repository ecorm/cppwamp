/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../streaming.hpp"

namespace wamp
{

//******************************************************************************
// CallerInputChunk
//******************************************************************************

CPPWAMP_INLINE CallerInputChunk::CallerInputChunk() {}

CPPWAMP_INLINE CallerInputChunk::CallerInputChunk(internal::PassKey,
                                                  internal::ResultMessage&& msg)
    : Base(std::move(msg))
{}


//******************************************************************************
// CallerOutputChunk
//******************************************************************************

/** This sets the `CALL.Options.progress|bool` option accordingly. */
CPPWAMP_INLINE CallerOutputChunk::CallerOutputChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
)
    : Base(isFinal)
{}

CPPWAMP_INLINE void CallerOutputChunk::setCallInfo(internal::PassKey,
                                                   String uri)
{
    message().setUri(std::move(uri));
}

CPPWAMP_INLINE internal::CallMessage&
CallerOutputChunk::callMessage(internal::PassKey, RequestId reqId)
{
    message().setRequestId(reqId);
    return message();
}


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
// Invitation
//******************************************************************************

/** Constructor. */
CPPWAMP_INLINE Invitation::Invitation(String uri, StreamMode mode)
    : Base(std::move(uri)),
      mode_(mode)
{
    using D = StreamMode;
    if (mode == D::calleeToCaller || mode == D::bidirectional)
        withProgressiveResults();
    if (mode == D::callerToCallee || mode == D::bidirectional)
        withProgress();
}

CPPWAMP_INLINE StreamMode Invitation::mode() const {return mode_;}

CPPWAMP_INLINE internal::CallMessage& Invitation::callMessage(internal::PassKey)
{
    return message();
}


//******************************************************************************
// Stream
//******************************************************************************

CPPWAMP_INLINE Stream::Stream(String uri): Base(std::move(uri)) {}

CPPWAMP_INLINE Stream& Stream::disableInvitation(bool disabled)
{
    invitationDisabled_ = disabled;
    return *this;
}

CPPWAMP_INLINE bool Stream::invitationDisabled() const
{
    return invitationDisabled_;
}

CPPWAMP_INLINE internal::RegisterMessage&
Stream::registerMessage(internal::PassKey)
{
    return message();
}


//******************************************************************************
// CallerChannel
//******************************************************************************

CPPWAMP_INLINE CallerChannel::~CallerChannel()
{
    auto oldState = state_.exchange(State::closed);
    if (oldState != State::closed)
        safeCancel();
}

CPPWAMP_INLINE StreamMode CallerChannel::mode() const {return mode_;}

CPPWAMP_INLINE bool CallerChannel::hasRsvp() const {return hasRsvp_;}

CPPWAMP_INLINE const CallerInputChunk& CallerChannel::rsvp() const &
{
    return rsvp_;
}

CPPWAMP_INLINE CallerInputChunk&& CallerChannel::rsvp() &&
{
    return std::move(rsvp_);
}

/** A CallerChannel is `open` upon creation and `closed` upon sending
    a final chunk or cancellation. */
CPPWAMP_INLINE CallerChannel::State CallerChannel::state() const
{
    return state_.load();
}

CPPWAMP_INLINE ChannelId CallerChannel::id() const {return id_;}

CPPWAMP_INLINE const Error& CallerChannel::error() const & {return error_;}

CPPWAMP_INLINE Error&& CallerChannel::error() && {return std::move(error_);}

/** The channel is closed if the given chunk is marked as final.
    @returns
        - false if the associated Session object is destroyed or
                the streaming request no longer exists
        - true if the chunk was accepted for processing
        - an error code if there was a problem processing the chunk
    @pre `this->state() == State::open`
    @pre `this->mode() == StreamMode::callerToCallee ||
          this->mode() == StreamMode::bidirectional`
    @post `chunk.isFinal() ? State::closed : State::open`
    @throws error::Logic if the mode precondition is not met */
CPPWAMP_INLINE ErrorOrDone CallerChannel::send(OutputChunk chunk)
{
    CPPWAMP_LOGIC_CHECK(isValidModeForSending(),
                        "wamp::CallerChannel::send: invalid mode");
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

/** @copydetails send(OutputChunk) */
CPPWAMP_INLINE std::future<ErrorOrDone> CallerChannel::send(ThreadSafe,
                                                            OutputChunk chunk)
{
    CPPWAMP_LOGIC_CHECK(isValidModeForSending(),
                        "wamp::CallerChannel::send: invalid mode");
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

/** @returns
        - false if the associated Session object is destroyed or
                the streaming request no longer exists
        - true if the cancellation was accepted for processing
        - an error code if there was a problem processing the cancellation
    @pre `this->state() == State::open`
    @post `this->state() == State::closed` */
CPPWAMP_INLINE ErrorOrDone CallerChannel::cancel(CallCancelMode mode)
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

/** @copydetails cancel(CallCancelMode) */
CPPWAMP_INLINE std::future<ErrorOrDone>
CallerChannel::cancel(ThreadSafe, CallCancelMode mode)
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

/** @copydetails cancel(CallCancelMode) */
CPPWAMP_INLINE ErrorOrDone CallerChannel::cancel() {return cancel(cancelMode_);}

/** @copydetails cancel(CallCancelMode) */
CPPWAMP_INLINE std::future<ErrorOrDone> CallerChannel::cancel(ThreadSafe)
{
    return cancel(threadSafe, cancelMode_);
}

CPPWAMP_INLINE std::future<ErrorOrDone> CallerChannel::futureValue(bool x)
{
    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(x);
    return f;
}

CPPWAMP_INLINE std::future<ErrorOrDone> CallerChannel::safeCancel()
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

CPPWAMP_INLINE CallerChannel::CallerChannel(
    internal::PassKey, const Invitation& inv,
    AnyReusableHandler<void (Ptr, ErrorOr<InputChunk>)> chunkHandler)
    : uri_(inv.uri()),
    chunkHandler_(std::move(chunkHandler)),
    cancelMode_(inv.cancelMode()),
    state_(State::open),
    mode_(inv.mode())
{}

CPPWAMP_INLINE void CallerChannel::init(internal::PassKey, ChannelId id,
                                        CallerPtr caller, AnyIoExecutor exec,
                                        AnyCompletionExecutor userExec)
{
    id_ = id;
    caller_ = std::move(caller);
    executor_ = std::move(exec);
    userExecutor_ = std::move(userExec);
}

CPPWAMP_INLINE bool CallerChannel::isValidModeForSending() const
{
    return mode_ == StreamMode::callerToCallee ||
           mode_ == StreamMode::bidirectional;
}

CPPWAMP_INLINE void CallerChannel::onRsvp(internal::PassKey,
                                          internal::ResultMessage&& msg)
{
    rsvp_ = InputChunk{{}, std::move(msg)};
    hasRsvp_ = true;
}

CPPWAMP_INLINE void CallerChannel::onReply(
    internal::PassKey, ErrorOr<internal::WampMessage>&& reply)
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
        InputChunk chunk{{}, std::move(msg)};
        dispatchChunkHandler(std::move(chunk));
    }
}


//******************************************************************************
// CalleeChannel
//******************************************************************************

CPPWAMP_INLINE CalleeChannel::~CalleeChannel()
{
    auto oldState = state_.exchange(State::closed);
    if (oldState != State::closed)
        safeSendChunk(CalleeOutputChunk{true});
}

CPPWAMP_INLINE StreamMode CalleeChannel::mode() const {return mode_;}

/** A CalleeChannel is `inviting` upon creation, `open` upon acceptance,
    and `closed` upon sending an error or a final chunk. */
CPPWAMP_INLINE CalleeChannel::State CalleeChannel::state() const
{
    return state_.load();
}

CPPWAMP_INLINE ChannelId CalleeChannel::id() const {return id_;}

CPPWAMP_INLINE bool CalleeChannel::invitationDisabled() const
{
    return invitationDisabled_;
}

CPPWAMP_INLINE const CalleeInputChunk& CalleeChannel::invitation() const &
{
    return invitation_;
}

CPPWAMP_INLINE CalleeInputChunk&& CalleeChannel::invitation() &&
{
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

    postInvitationAsChunkIfIgnored();

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

    postInvitationAsChunkIfIgnored();

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
    postInvitationAsChunkIfIgnored();

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

/** @copydetails CalleeChannel::send(OutputChunk) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::send(ThreadSafe,
                                                            OutputChunk chunk)
{
    State expectedState = State::inviting;
    auto newState = chunk.isFinal() ? State::closed : State::open;
    bool ok = state_.compare_exchange_strong(expectedState, newState);
    if (!ok)
        return futureError(Errc::invalidState);
    return safeSendChunk(std::move(chunk));
}

/** @returns
        - false if the associated Session object is destroyed or the
                channel state is already closed
        - true if the error was accepted for processing
        - an error code if there was a problem processing the error
    @post `this->state() == State::closed` */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::close(Error error)
{
    auto oldState = state_.exchange(State::closed);
    auto caller = callee_.lock();
    if (!caller || oldState == State::closed)
        return false;
    return caller->yield(id_, std::move(error));
}

/** @copydetails CalleeChannel::close(Error) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::close(ThreadSafe,
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

CPPWAMP_INLINE bool CalleeChannel::isValidModeFor(const OutputChunk& c) const
{
    using M = StreamMode;
    return c.isFinal() ||
           (mode_ == M::calleeToCaller) ||
           (mode_ == M::bidirectional);
}

CPPWAMP_INLINE std::future<ErrorOrDone>
CalleeChannel::safeSendChunk(OutputChunk&& chunk)
{
    auto caller = callee_.lock();
    if (!caller)
        return futureValue(false);
    return caller->safeSendCalleeChunk(id_, std::move(chunk));
}

CPPWAMP_INLINE void CalleeChannel::postInvitationAsChunkIfIgnored()
{
    if (chunkHandler_ && invitationDisabled_)
    {
        postVia(executor_, userExecutor_, chunkHandler_,
                shared_from_this(), std::move(invitation_));
        invitation_ = InputChunk{};
    }
}

CPPWAMP_INLINE CalleeChannel::CalleeChannel(
    internal::PassKey, internal::InvocationMessage&& msg,
    bool invitationDisabled, AnyIoExecutor executor,
    AnyCompletionExecutor userExecutor, CalleePtr callee)
    : invitation_({}, std::move(msg)),
      executor_(std::move(executor)),
      userExecutor_(std::move(userExecutor)),
      callee_(std::move(callee)),
      state_(State::inviting),
      mode_(invitation_.mode({})),
      invitationDisabled_(invitationDisabled)
{}

CPPWAMP_INLINE bool CalleeChannel::hasInterruptHandler(internal::PassKey) const
{
    return interruptHandler_ != nullptr;
}

CPPWAMP_INLINE void CalleeChannel::onInvocation(
    internal::PassKey, internal::InvocationMessage&& msg)
{
    if (!chunkHandler_)
        return;
    InputChunk chunk{{}, std::move(msg)};
    postVia(executor_, userExecutor_, chunkHandler_, shared_from_this(),
            std::move(chunk));
}

CPPWAMP_INLINE void CalleeChannel::onInterrupt(internal::PassKey,
                                               internal::InterruptMessage&& msg)
{
    if (!interruptHandler_)
        return;
    Interruption intr{{}, std::move(msg)};
    postVia(executor_, userExecutor_, interruptHandler_, shared_from_this(),
            std::move(intr));
}

} // namespace wamp
