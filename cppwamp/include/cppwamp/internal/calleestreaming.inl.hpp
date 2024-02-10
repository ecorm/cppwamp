/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../calleestreaming.hpp"
#include "streamchannel.hpp"

namespace wamp
{

//******************************************************************************
// Stream
//******************************************************************************

CPPWAMP_INLINE Stream::Stream(Uri uri)
    : Base(std::move(uri)),
      invitationExpected_(false)
{}

/** @details
    If unspecified, it is treated as a chunk. */
CPPWAMP_INLINE Stream& Stream::withInvitationExpected(bool expected)
{
    invitationExpected_ = expected;
    return *this;
}

CPPWAMP_INLINE bool Stream::invitationExpected() const
{
    return invitationExpected_;
}


//******************************************************************************
// CalleeChannel
//******************************************************************************

/** @post this->state() == State::detached */
CPPWAMP_INLINE CalleeChannel::CalleeChannel() = default;

CPPWAMP_INLINE StreamMode CalleeChannel::mode() const
{
    return impl_ ? impl_->mode() : StreamMode::unknown;
}

/** A CalleeChannel is `awaiting` upon establishment, `open` upon acceptance,
    and `closed` upon sending an error or a final chunk. */
CPPWAMP_INLINE CalleeChannel::State CalleeChannel::state() const
{
    return impl_ ? impl_->state() : State::detached;
}

CPPWAMP_INLINE ChannelId CalleeChannel::id() const
{
    return impl_ ? impl_->id() : nullId();
}

CPPWAMP_INLINE bool CalleeChannel::invitationExpected() const
{
    return impl_ && impl_->invitationExpected();
}

CPPWAMP_INLINE const CalleeInputChunk& CalleeChannel::invitation() const &
{
    static const CalleeInputChunk empty;
    return impl_ ? impl_->invitation() : empty;
}

/** @pre `this->attached() && this->invitationExpected()`
    @throws error::Logic if the precondition is not met. */
CPPWAMP_INLINE CalleeInputChunk&& CalleeChannel::invitation() &&
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::invitation: "
                                    "Cannot move from detached channel");
    return std::move(*impl_).invitation();
}

/** Obtains the executor used to execute user-provided handlers. */
CPPWAMP_INLINE const AnyCompletionExecutor& CalleeChannel::fallbackExecutor() const
{
    return impl_->fallbackExecutor();
}

CPPWAMP_INLINE bool CalleeChannel::attached() const
{
    return static_cast<bool>(impl_);
}

CPPWAMP_INLINE CalleeChannel::operator bool() const {return attached();}

/** @details
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
CPPWAMP_INLINE ErrorOrDone CalleeChannel::respond(
    OutputChunk response, ChunkSlot chunkSlot, InterruptSlot interruptSlot)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::accept: "
                                    "Channel is detached");
    return impl_->respond(std::move(response), std::move(chunkSlot),
                          std::move(interruptSlot));
}

/** @copydetails CalleeChannel::respond */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::accept(ChunkSlot chunkSlot,
                                                 InterruptSlot interruptSlot)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::accept: "
                                    "Channel is detached");
    return impl_->accept(std::move(chunkSlot), std::move(interruptSlot));
}

/** The channel is closed if the given chunk is marked as final. */
/** @returns
        - false if the associated Session object is destroyed
        - true if the chunk was accepted for processing
        - an error code if there was a problem processing the chunk
    @pre `this->attached() == true`
    @pre `chunk.isFinal() || this->mode == StreamMode::calleeToCaller ||
          this->mode == StreamMode::bidirectional`
    @post `this->state() == chunk.isFinal() ? State::closed : State::open`
    @throws error::Logic if the mode precondition is not met */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::send(OutputChunk chunk)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::send: "
                                    "Channel is detached");
    return impl_->send(std::move(chunk));
}

/** @details
    Does nothing if the associated Session object is destroyed or the channel
    state is already closed.
    @pre `this->attached() == true`
    @post `this->state() == State::closed` */
CPPWAMP_INLINE void CalleeChannel::fail(Error error)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::fail: "
                                    "Channel is detached");
    if (impl_)
        impl_->fail(std::move(error));
}

/** @post this->state() == State::detached */
CPPWAMP_INLINE void CalleeChannel::detach() {impl_.reset();}

CPPWAMP_INLINE CalleeChannel::CalleeChannel(internal::PassKey,
                                            std::shared_ptr<Impl> impl)
    : impl_(std::move(impl))
{}

} // namespace wamp
