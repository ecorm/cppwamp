/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../callerstreaming.hpp"

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
// Invitation
//******************************************************************************

/** Constructor. */
CPPWAMP_INLINE Invitation::Invitation(String uri, StreamMode mode)
    : Base(std::move(uri)),
      mode_(mode)
{
    using D = StreamMode;
    if (mode == D::calleeToCaller || mode == D::bidirectional)
        withOption("receive_progress", true);
    if (mode == D::callerToCallee || mode == D::bidirectional)
        withOption("progress", true);
}

CPPWAMP_INLINE StreamMode Invitation::mode() const {return mode_;}

CPPWAMP_INLINE internal::CallMessage& Invitation::callMessage(internal::PassKey,
                                                              RequestId reqId)
{
    message().setRequestId(reqId);
    return message();
}


//******************************************************************************
// Summons
//******************************************************************************

/** Constructor. */
CPPWAMP_INLINE Summons::Summons(String uri, StreamMode mode)
    : Base(std::move(uri)),
      mode_(mode)
{
    using D = StreamMode;
    if (mode == D::calleeToCaller || mode == D::bidirectional)
        withOption("receive_progress", true);
    if (mode == D::callerToCallee || mode == D::bidirectional)
        withOption("progress", true);
}

CPPWAMP_INLINE StreamMode Summons::mode() const {return mode_;}

CPPWAMP_INLINE internal::CallMessage& Summons::callMessage(internal::PassKey,
                                                           RequestId reqId)
{
    message().setRequestId(reqId);
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

CPPWAMP_INLINE CallerChannel::CallerChannel(
    ChannelId id, String&& uri, StreamMode mode, CallCancelMode cancelMode,
    CallerPtr caller,
    AnyReusableHandler<void (Ptr, ErrorOr<InputChunk>)>&& onChunk,
    AnyIoExecutor exec, AnyCompletionExecutor userExec)
    : uri_(std::move(uri)),
      chunkSlot_(std::move(onChunk)),
      executor_(exec),
      userExecutor_(std::move(userExec)),
      caller_(std::move(caller)),
      id_(id),
      state_(State::open),
      mode_(mode),
      cancelMode_(cancelMode)
{}

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

CPPWAMP_INLINE CallerChannel::Ptr
CallerChannel::create(
    internal::PassKey, ChannelId id, String&& uri, StreamMode mode,
    CallCancelMode cancelMode, CallerPtr caller,
    AnyReusableHandler<void (Ptr, ErrorOr<InputChunk>)>&& onChunk,
    AnyIoExecutor exec, AnyCompletionExecutor userExec)
{
    return Ptr(new CallerChannel(id, std::move(uri), mode, cancelMode,
                                 std::move(caller), std::move(onChunk),
                                 std::move(exec), std::move(userExec)));
}

CPPWAMP_INLINE bool CallerChannel::isValidModeForSending() const
{
    return mode_ == StreamMode::callerToCallee ||
           mode_ == StreamMode::bidirectional;
}

CPPWAMP_INLINE void CallerChannel::setRsvp(internal::PassKey,
                                          internal::ResultMessage&& msg)
{
    rsvp_ = InputChunk{{}, std::move(msg)};
    hasRsvp_ = true;
}

CPPWAMP_INLINE void CallerChannel::postResult(internal::PassKey,
                                              internal::ResultMessage&& msg)
{
    if (!chunkSlot_)
        return;
    InputChunk chunk{{}, std::move(msg)};
    postChunkHandler(std::move(chunk));
}

CPPWAMP_INLINE void CallerChannel::postError(internal::PassKey,
                                             internal::ErrorMessage&& msg)
{
    if (!chunkSlot_)
        return;
    error_ = Error{{}, std::move(msg)};
    auto errc = error_.errorCode();
    postChunkHandler(makeUnexpectedError(errc));
}

} // namespace wamp
