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
#include <memory>
#include "../anyhandler.hpp"
#include "../asiodefs.hpp"
#include "../rpcinfo.hpp"
#include "../streaming.hpp"
#include "callee.hpp"
#include "caller.hpp"
#include "message.hpp"
#include "passkey.hpp"

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
    using Executor = AnyIoExecutor;
    using FallbackExecutor = AnyCompletionExecutor;
    using State = ChannelState;

    BasicCallerChannelImpl(
        ChannelId id, String&& uri, StreamMode mode, CallCancelMode cancelMode,
        bool expectsRsvp, Caller::WeakPtr caller, ChunkSlot&& onChunk,
        AnyIoExecutor exec, FallbackExecutor userExec)
        : uri_(std::move(uri)),
          chunkSlot_(std::move(onChunk)),
          executor_(std::move(exec)),
          userExecutor_(std::move(userExec)),
          caller_(std::move(caller)),
          id_(id),
          state_(State::open),
          mode_(mode),
          cancelMode_(cancelMode),
          expectsRsvp_(expectsRsvp)
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

    const Executor& executor() const {return executor_;}

    const FallbackExecutor& userExecutor() const {return userExecutor_;}

    CPPWAMP_NODISCARD ErrorOrDone send(OutputChunk chunk)
    {
        CPPWAMP_LOGIC_CHECK(isValidModeForSending(),
                            "wamp::CallerChannel::send: invalid mode");
        State expectedState = State::open;
        auto newState = chunk.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return makeUnexpectedError(MiscErrc::invalidState);

        auto caller = caller_.lock();
        if (!caller)
            return false;
        chunk.setCallInfo({}, id_, uri_);
        return caller->safeSendCallerChunk(std::move(chunk)).get();
    }

    ErrorOrDone cancel(CallCancelMode mode)
    {
        State expectedState = State::open;
        bool ok = state_.compare_exchange_strong(expectedState, State::closed);
        if (!ok)
            return makeUnexpectedError(MiscErrc::invalidState);

        auto caller = caller_.lock();
        if (!caller)
            return false;
        return caller->safeCancelCall(id_, mode).get();
    }

    ErrorOrDone cancel() {return cancel(cancelMode_);}

    bool expectsRsvp() const {return expectsRsvp_;}

    void setRsvp(Message&& msg)
    {
        rsvp_ = InputChunk{{}, std::move(msg)};
        hasRsvp_ = true;
    }

    void postResult(Message&& msg)
    {
        if (!chunkSlot_)
            return;
        InputChunk chunk{{}, std::move(msg)};
        postToChunkHandler(std::move(chunk));
    }

    void postError(Message&& msg)
    {
        if (!chunkSlot_)
            return;
        error_ = Error{{}, std::move(msg)};
        auto errc = error_.errorCode();
        postToChunkHandler(makeUnexpectedError(errc));
    }

    void postError(UnexpectedError unex)
    {
        error_ = Error{unex.value()};
        postToChunkHandler(unex);
    }

private:
    void safeCancel()
    {
        auto caller = caller_.lock();
        if (caller)
            caller->safeCancelStream(id_);
    }

    void postToChunkHandler(ErrorOr<InputChunk>&& errorOrChunk)
    {
        struct Posted
        {
            Ptr self;
            ChunkSlot slot;
            ErrorOr<InputChunk> errorOrChunk;

            void operator()()
            {
                auto& me = *self;
                try
                {
                    slot(TContext{{}, self}, std::move(errorOrChunk));
                }
                catch (Error& error)
                {
                    me.error_ = std::move(error);
                    me.safeCancel();
                }
                catch (const error::BadType& e)
                {
                    me.error_ = Error{e};
                    me.safeCancel();
                }
            }
        };

        auto associatedExec =
            boost::asio::get_associated_executor(chunkSlot_, userExecutor_);
        Posted posted{this->shared_from_this(), chunkSlot_,
                      std::move(errorOrChunk)};
        boost::asio::post(
            executor_,
            boost::asio::bind_executor(associatedExec, std::move(posted)));
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
    Executor executor_;
    FallbackExecutor userExecutor_;
    Caller::WeakPtr caller_;
    ChannelId id_ = nullId();
    std::atomic<State> state_;
    StreamMode mode_ = {};
    CallCancelMode cancelMode_ = CallCancelMode::unknown;
    bool expectsRsvp_ = false;
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
    using State = ChannelState;

    BasicCalleeChannelImpl(
        Invocation&& inv, bool invitationExpected, Executor executor)
        : registrationId_(inv.registrationId()),
          invitation_({}, std::move(inv)),
          executor_(std::move(executor)),
          userExecutor_(inv.executor()),
          callee_(inv.callee({})),
          id_(invitation_.channelId()),
          state_(State::awaiting),
          mode_(invitation_.mode({})),
          invitationExpected_(invitationExpected)
    {}

    ~BasicCalleeChannelImpl()
    {
        chunkSlot_ = nullptr;
        interruptSlot_ = nullptr;
        fail(Error{WampErrc::cancelled});
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

    const FallbackExecutor& userExecutor() const {return userExecutor_;}

    ErrorOrDone respond(OutputChunk response, ChunkSlot onChunk = {},
                        InterruptSlot onInterrupt = {})
    {
        CPPWAMP_LOGIC_CHECK(isValidModeFor(response),
                            "wamp::CalleeChannel::accept: invalid mode");
        State expectedState = State::awaiting;
        auto newState = response.isFinal() ? State::closed : State::open;
        bool ok = state_.compare_exchange_strong(expectedState, newState);
        if (!ok)
            return makeUnexpectedError(MiscErrc::invalidState);

        if (!response.isFinal())
        {
            chunkSlot_ = std::move(onChunk);
            interruptSlot_ = std::move(onInterrupt);
        }

        postUnexpectedInvitationAsChunk();

        auto caller = callee_.lock();
        if (!caller)
            return false;
        return caller->safeYield(std::move(response), id_,
                                 registrationId_).get();
    }

    CPPWAMP_NODISCARD ErrorOrDone accept(ChunkSlot onChunk = {},
                                         InterruptSlot onInterrupt = {})
    {
        State expectedState = State::awaiting;
        bool ok = state_.compare_exchange_strong(expectedState, State::open);
        if (!ok)
            return makeUnexpectedError(MiscErrc::invalidState);

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
            return makeUnexpectedError(MiscErrc::invalidState);
        auto callee = callee_.lock();
        if (!callee)
            return false;
        return callee->safeYield(std::move(chunk), id_, registrationId_).get();
    }

    ErrorOrDone fail(Error error)
    {
        auto oldState = state_.exchange(State::closed);
        auto caller = callee_.lock();
        if (!caller || oldState == State::closed)
            return false;
        return caller->safeYield(std::move(error), id_, registrationId_).get();
    }

    bool hasInterruptHandler() const
    {
        return interruptSlot_ != nullptr;
    }

    void postInvocation(Invocation&& inv)
    {
        InputChunk chunk{{}, std::move(inv)};
        postToSlot(chunkSlot_, std::move(chunk));
    }

    bool postInterrupt(Interruption&& intr)
    {
        return postToSlot(interruptSlot_, std::move(intr));
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
            postAny(executor_, chunkSlot_,
                    TContext{{}, this->shared_from_this()},
                    std::move(invitation_));
            invitation_ = InputChunk{};
        }
    }

    template <typename S, typename T>
    bool postToSlot(const S& slot, T&& request)
    {
        struct Posted
        {
            Ptr self;
            S slot;
            ValueTypeOf<T> arg;

            void operator()()
            {
                try
                {
                    slot(TContext{{}, self}, std::move(arg));
                }
                catch (Error& error)
                {
                    self->fail(std::move(error));
                }
                catch (const error::BadType& e)
                {
                    // Forward Variant conversion exceptions as ERROR messages.
                    self->fail(Error(e));
                }
            }
        };

        if (!slot)
            return false;

        auto associatedExec =
            boost::asio::get_associated_executor(chunkSlot_, userExecutor_);
        Posted posted{this->shared_from_this(), slot, std::move(request)};
        boost::asio::post(
            executor_,
            boost::asio::bind_executor(associatedExec, std::move(posted)));
        return true;
    }

    RegistrationId registrationId_;
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
