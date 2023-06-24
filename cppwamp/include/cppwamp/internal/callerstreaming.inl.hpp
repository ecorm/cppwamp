/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../callerstreaming.hpp"
#include "../exceptions.hpp"
#include "streamchannel.hpp"

namespace wamp
{

//******************************************************************************
// StreamRequest
//******************************************************************************

/** Constructor. */
CPPWAMP_INLINE StreamRequest::StreamRequest(Uri uri, StreamMode mode)
    : Base(std::move(uri)),
      mode_(mode)
{
    using M = StreamMode;
    CPPWAMP_LOGIC_CHECK(
        mode != M::unknown,
        "wamp::StreamRequest::StreamRequest: Cannot specify unknown mode");
    if (mode == M::calleeToCaller || mode == M::bidirectional)
        withOption("receive_progress", true);
    if (mode == M::callerToCallee || mode == M::bidirectional)
        withOption("progress", true);
}

CPPWAMP_INLINE StreamMode StreamRequest::mode() const {return mode_;}


//******************************************************************************
// CallerChannel
//******************************************************************************

/** @post this->state() == State::detached */
CPPWAMP_INLINE CallerChannel::CallerChannel() = default;

CPPWAMP_INLINE StreamMode CallerChannel::mode() const
{
    return impl_ ? impl_->mode() : StreamMode::unknown;
}

CPPWAMP_INLINE bool CallerChannel::hasRsvp() const
{
    return impl_ && impl_->hasRsvp();
}

CPPWAMP_INLINE const CallerInputChunk& CallerChannel::rsvp() const &
{
    static const CallerInputChunk empty;
    return impl_ ? impl_->rsvp() : empty;
}

/** @pre `this->attached()`
    @throws error::Logic if the precondition is not met. */
CPPWAMP_INLINE CallerInputChunk&& CallerChannel::rsvp() &&
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CallerChannel::rsvp: "
                                    "Cannot move from detached channel");
    return std::move(*impl_).rsvp();
}

/** A CallerChannel is `open` upon establishment and `closed` upon sending
    a final chunk or cancellation. */
CPPWAMP_INLINE CallerChannel::State CallerChannel::state() const
{
    return impl_ ? impl_->state() : State::detached;
}

CPPWAMP_INLINE ChannelId CallerChannel::id() const
{
    return impl_ ? impl_->id() : nullId();
}

CPPWAMP_INLINE const Error& CallerChannel::error() const &
{
    static const Error empty;
    return impl_ ? impl_->error() : empty;
}

/** @pre `this->attached()`
    @throws error::Logic if the precondition is not met. */
CPPWAMP_INLINE Error&& CallerChannel::error() &&
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CallerChannel::error: "
                                    "Cannot move from detached channel");
    return std::move(*impl_).error();
}

CPPWAMP_INLINE bool CallerChannel::attached() const
{
    return static_cast<bool>(impl_);
}

CPPWAMP_INLINE CallerChannel::operator bool() const {return attached();}

/** @details
    The channel is closed if the given chunk is marked as final.
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
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CallerChannel::send: "
                                    "Channel is detached");
    return impl_->send(std::move(chunk));
}

/** @details
    Does nothing if the associated Session object is destroyed, the channel
    state is already closed, or the streaming request no longer exists.
    @pre `this->attached() == true`
    @post `this->state() == State::closed` */
CPPWAMP_INLINE void CallerChannel::cancel(CallCancelMode mode)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CallerChannel::cancel: "
                                    "Channel is detached");
    impl_->cancel(mode);
}

/** @copydetails cancel(CallCancelMode) */
CPPWAMP_INLINE void CallerChannel::cancel()
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CallerChannel::cancel: "
                                    "Channel is detached");
    impl_->cancel();
}

/** @post this->state() == State::detached */
CPPWAMP_INLINE void CallerChannel::detach() {impl_.reset();}

CPPWAMP_INLINE CallerChannel::CallerChannel(internal::PassKey,
                                            std::shared_ptr<Impl> impl)
    : impl_(std::move(impl))
{}

} // namespace wamp
