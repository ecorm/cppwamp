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

/** @post this->state() == State::detached */
CPPWAMP_INLINE CalleeChannel::CalleeChannel() {}

CPPWAMP_INLINE StreamMode CalleeChannel::mode() const
{
    return impl_ ? impl_->mode() : StreamMode::unknown;
}

/** A CalleeChannel is `inviting` upon establishment, `open` upon acceptance,
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

CPPWAMP_INLINE bool CalleeChannel::attached() const {return bool(impl_);}

CPPWAMP_INLINE CalleeChannel::operator bool() const {return attached();}

/** The channel is immediately closed if the given chunk is marked as final.
    @returns
        - false if the associated Session object is destroyed or
                the streaming request no longer exists
        - true if the response was accepted for processing
        - an error code if there was a problem processing the response
    @note This method should be called within the invocation context of the
          StreamSlot in order to losing incoming chunks or interruptions due
          to the ChunkSlot or InterruptSlot not being registered in time.
    @pre `this->state() == State::inviting`
    @pre `response.isFinal() || this->mode == StreamMode::calleeToCaller ||
          this->mode == StreamMode::bidirectional`
    @post `this->state() == response.isFinal() ? State::closed : State::open`
    @throws error::Logic if the mode precondition is not met */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::accept(
    OutputChunk response,     ///< The RSVP to return back to the caller.
    ChunkSlot onChunk,        ///< Optional handler to use for received chunks.
    InterruptSlot onInterrupt ///< Optional handler to use for interruptions.
    )
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::accept: "
                                    "Channel is detached");
    return impl_->accept(std::move(response), std::move(onChunk),
                         std::move(onInterrupt));
}

/** @copydetails CalleeChannel::accept(OutputChunk, ChunkSlot, InterruptSlot) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::accept(
    ThreadSafe,
    OutputChunk response,     ///< The RSVP to return back to the caller.
    ChunkSlot onChunk,        ///< Optional handler to use for received chunks.
    InterruptSlot onInterrupt ///< Optional handler to use for interruptions.
    )
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::accept: "
                                    "Channel is detached");
    return impl_->accept(threadSafe, std::move(response), std::move(onChunk),
                         std::move(onInterrupt));
}

/** This function is thread-safe.
    @returns an error code if the channel was not in the inviting state
    @note In order to not lose any incoming chunks or interruptions, this
          method should be called within the context of the StreamSlot
          registered via Session::enroll.
    @pre `this->state() == State::inviting`
    @post `this->state() == State::open` */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::accept(
    ChunkSlot onChunk,        ///< Handler to use for received chunks.
    InterruptSlot onInterrupt ///< Handler to use for received interruptions.
    )
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::accept: "
                                    "Channel is detached");
    return impl_->accept(std::move(onChunk), std::move(onInterrupt));
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
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::send: "
                                    "Channel is detached");
    return impl_->send(std::move(chunk));
}

/** @copydetails CalleeChannel::send(OutputChunk) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::send(ThreadSafe,
                                                            OutputChunk chunk)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::send: "
                                    "Channel is detached");
    return impl_->send(threadSafe, std::move(chunk));
}

/** @returns
        - false if the associated Session object is destroyed or the
                channel state is already detached or closed
        - true if the error was accepted for processing
        - an error code if there was a problem processing the error
    @pre `this->attached() == true`
    @post `this->state() == State::closed` */
CPPWAMP_INLINE ErrorOrDone CalleeChannel::fail(Error error)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::fail: "
                                    "Channel is detached");
    return impl_->fail(std::move(error));
}

/** @copydetails CalleeChannel::fail(Error) */
CPPWAMP_INLINE std::future<ErrorOrDone> CalleeChannel::fail(ThreadSafe,
                                                            Error error)
{
    CPPWAMP_LOGIC_CHECK(attached(), "wamp::CalleeChannel::fail: "
                                    "Channel is detached");
    if (!impl_)
    {
        std::promise<ErrorOrDone> p;
        auto f = p.get_future();
        p.set_value(false);
        return f;
    }

    return impl_->fail(threadSafe, std::move(error));
}

/** @post this->state() == State::detached */
CPPWAMP_INLINE void CalleeChannel::detach() {impl_.reset();}

CPPWAMP_INLINE CalleeChannel::CalleeChannel(internal::PassKey,
                                            std::shared_ptr<Impl> impl)
    : impl_(std::move(impl))
{}

} // namespace wamp
