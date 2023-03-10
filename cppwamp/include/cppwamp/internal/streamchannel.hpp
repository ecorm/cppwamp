/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_STREAM_CHANNEL_HPP
#define CPPWAMP_INTERNAL_STREAM_CHANNEL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for steaming chunks to/from callers. */
//------------------------------------------------------------------------------

#include <atomic>
#include <future>
#include <memory>
#include "../anyhandler.hpp"
#include "../asiodefs.hpp"
#include "../peerdata.hpp"
#include "../streaming.hpp"
#include "../tagtypes.hpp"
#include "callee.hpp"
#include "caller.hpp"
#include "passkey.hpp"
#include "wampmessage.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TContext>
class BasicCallerChannelImpl
    : public std::enable_shared_from_this<BasicCallerChannelImpl<TContext>>
{
public:
    using Ptr = std::shared_ptr<BasicCallerChannelImpl>;
    using WeakPtr = std::weak_ptr<BasicCallerChannelImpl>;
    using InputChunk = CallerInputChunk;
    using OutputChunk = CallerOutputChunk;
    using ChunkSlot = AnyReusableHandler<void (TContext, ErrorOr<InputChunk>)>;

    using State = CallerChannelState;

    BasicCallerChannelImpl(
        ChannelId id, String&& uri, StreamMode mode,
        CallCancelMode cancelMode, Caller::WeakPtr caller, ChunkSlot&& onChunk,
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

    ~BasicCallerChannelImpl()
    {
        auto oldState = state_.exchange(State::closed);
        if (oldState != State::closed)
            safeCancel();
    }

    StreamMode mode() const {return mode_;}

    bool hasRsvp() const {return hasRsvp_;}

    const InputChunk& rsvp() const & {return rsvp_;}

    InputChunk&& rsvp() && {return std::move(rsvp_);}

    State state() const {return state_.load();}

    ChannelId id() const {return id_;}

    const Error& error() const & {return error_;}

    Error&& error() && {return std::move(error_);}

    CPPWAMP_NODISCARD ErrorOrDone send(OutputChunk chunk)
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

    CPPWAMP_NODISCARD std::future<ErrorOrDone> send(ThreadSafe,
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

    ErrorOrDone cancel()
    {
        return cancel(cancelMode_);
    }

    std::future<ErrorOrDone> cancel(ThreadSafe)
    {
        return cancel(threadSafe, cancelMode_);
    }

    void setRsvp(ResultMessage&& msg)
    {
        rsvp_ = InputChunk{{}, std::move(msg)};
        hasRsvp_ = true;
    }

    void postResult(ResultMessage&& msg)
    {
        if (!chunkSlot_)
            return;
        InputChunk chunk{{}, std::move(msg)};
        postChunkHandler(std::move(chunk));
    }

    void postError(ErrorMessage&& msg)
    {
        if (!chunkSlot_)
            return;
        error_ = Error{{}, std::move(msg)};
        auto errc = error_.errorCode();
        postChunkHandler(makeUnexpectedError(errc));
    }

private:
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
    void postChunkHandler(T&& arg)
    {
        postVia(executor_, userExecutor_, chunkSlot_,
                TContext{{}, this->shared_from_this()}, std::forward<T>(arg));
    }

    bool isValidModeForSending() const
    {
        return mode_ == StreamMode::callerToCallee ||
               mode_ == StreamMode::bidirectional;
    }

    InputChunk rsvp_;
    Error error_;
    Uri uri_;
    ChunkSlot chunkSlot_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    Caller::WeakPtr caller_;
    ChannelId id_ = nullId();
    std::atomic<State> state_;
    StreamMode mode_ = {};
    CallCancelMode cancelMode_ = CallCancelMode::unknown;
    bool hasRsvp_ = false;
};


//------------------------------------------------------------------------------
template <typename TContext>
class BasicCalleeChannelImpl
    : public std::enable_shared_from_this<BasicCalleeChannelImpl<TContext>>
{
public:
    using Ptr = std::shared_ptr<BasicCalleeChannelImpl>;
    using WeakPtr = std::weak_ptr<BasicCalleeChannelImpl>;
    using InputChunk = CalleeInputChunk;
    using OutputChunk = CalleeOutputChunk;
    using Executor = AnyIoExecutor;
    using FallbackExecutor = AnyCompletionExecutor;
    using ChunkSlot = AnyReusableHandler<void (TContext, InputChunk)>;
    using InterruptSlot = AnyReusableHandler<void (TContext, Interruption)>;
    using State = CalleeChannelState;

    BasicCalleeChannelImpl(
        internal::InvocationMessage&& msg, bool invitationExpected,
        AnyIoExecutor executor, AnyCompletionExecutor userExecutor,
        Callee::WeakPtr callee)
        : invitation_({}, std::move(msg)),
          executor_(std::move(executor)),
          userExecutor_(std::move(userExecutor)),
          callee_(std::move(callee)),
          id_(invitation_.channelId()),
          state_(State::closed),
          mode_(invitation_.mode({})),
          invitationExpected_(invitationExpected)
    {}

    ~BasicCalleeChannelImpl()
    {
        fail(threadSafe, Error{WampErrc::cancelled});
    }

    StreamMode mode() const {return mode_;}

    State state() const {return state_.load();}

    ChannelId id() const {return id_;}

    bool invitationExpected() const {return invitationExpected_;}

    const InputChunk& invitation() const &
    {
        static const CalleeInputChunk empty;
        return invitationExpected_ ? invitation_ : empty;
    }

    InputChunk&& invitation() &&
    {
        CPPWAMP_LOGIC_CHECK(
            invitationExpected_,
            "wamp::CalleeChannel::invitation: cannot move unexpected invitation");
        return std::move(invitation_);
    }

    const Executor& executor() const {return executor_;}

    const FallbackExecutor& fallbackExecutor() const {return userExecutor_;}

    ErrorOrDone accept(OutputChunk response, ChunkSlot onChunk = {},
                       InterruptSlot onInterrupt = {})
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
            chunkSlot_ = std::move(onChunk);
            interruptSlot_ = std::move(onInterrupt);
        }

        postUnexpectedInvitationAsChunk();

        auto caller = callee_.lock();
        if (!caller)
            return false;
        return caller->sendCalleeChunk(id_, std::move(response));
    }

    std::future<ErrorOrDone> accept(
        ThreadSafe, OutputChunk response, ChunkSlot onChunk = {},
        InterruptSlot onInterrupt = {})
    {
        State expectedState = State::inviting;
        auto newState = response.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return futureError(Errc::invalidState);

        if (!response.isFinal())
        {
            chunkSlot_ = std::move(onChunk);
            interruptSlot_ = std::move(onInterrupt);
        }

        postUnexpectedInvitationAsChunk();

        auto caller = callee_.lock();
        if (!caller)
            return futureValue(false);
        return caller->safeSendCalleeChunk(id_, std::move(response));
    }

    CPPWAMP_NODISCARD ErrorOrDone accept(ChunkSlot onChunk = {},
                                         InterruptSlot onInterrupt = {})
    {
        State expectedState = State::inviting;
        bool ok = state_.compare_exchange_strong(expectedState, State::open);
        if (!ok)
            return makeUnexpectedError(Errc::invalidState);

        chunkSlot_ = std::move(onChunk);
        interruptSlot_ = std::move(onInterrupt);
        postUnexpectedInvitationAsChunk();

        return true;
    }

    CPPWAMP_NODISCARD ErrorOrDone send(OutputChunk chunk)
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

    CPPWAMP_NODISCARD std::future<ErrorOrDone> send(ThreadSafe,
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

    ErrorOrDone fail(Error error)
    {
        auto oldState = state_.exchange(State::closed);
        auto caller = callee_.lock();
        if (!caller || oldState == State::closed)
            return false;
        return caller->yield(id_, std::move(error));
    }

    std::future<ErrorOrDone> fail(ThreadSafe, Error error)
    {
        auto oldState = state_.exchange(State::closed);
        auto caller = callee_.lock();
        if (!caller || oldState == State::closed)
            return futureValue(false);
        return caller->safeYield(id_, std::move(error));
    }

    bool hasInterruptHandler() const
    {
        return interruptSlot_ != nullptr;
    }

    void postInvocation(InvocationMessage&& msg)
    {
        if (!chunkSlot_)
            return;
        InputChunk chunk{{}, std::move(msg)};
        postVia(executor_, userExecutor_, chunkSlot_,
                TContext{{}, this->shared_from_this()}, std::move(chunk));
    }

    bool postInterrupt(InterruptMessage&& msg)
    {
        if (!interruptSlot_)
            return false;
        Interruption intr{{}, std::move(msg)};
        postVia(executor_, userExecutor_, interruptSlot_,
                TContext{{}, this->shared_from_this()}, std::move(intr));
        return true;
    }

private:
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

    bool isValidModeFor(const OutputChunk& c) const
    {
        using M = StreamMode;
        return c.isFinal() ||
               (mode_ == M::calleeToCaller) ||
               (mode_ == M::bidirectional);
    }

    void postUnexpectedInvitationAsChunk()
    {
        if (chunkSlot_ && !invitationExpected_)
        {
            postVia(executor_, userExecutor_, chunkSlot_,
                    TContext{{}, this->shared_from_this()},
                    std::move(invitation_));
            invitation_ = InputChunk{};
        }
    }

    InputChunk invitation_;
    ChunkSlot chunkSlot_;
    InterruptSlot interruptSlot_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor userExecutor_;
    Callee::WeakPtr callee_;
    ChannelId id_ = nullId();
    std::atomic<State> state_;
    StreamMode mode_ = {};
    bool invitationExpected_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_STREAM_CHANNEL_HPP
