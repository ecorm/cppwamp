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

    using Base::withProgressiveResults;
    using Base::progressiveResultsAreEnabled;
    using Base::withProgress;
    using Base::isProgress;

    Direction direction_;

public:
    // Internal use only
    internal::CallMessage& callMessage(internal::PassKey);
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
/** Provides the interface for a caller to stream chunks of data. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerChannel
    : public std::enable_shared_from_this<CallerChannel>
{
public:
    using Ptr = std::shared_ptr<CallerChannel>;
    using Chunk = CallerChunk;

    ~CallerChannel() {close(threadSafe);}

    /** Returns true if the channel is open. */
    explicit operator bool() const {return isOpen();}

    /** Determines if the channel is open.
        This channel is open upon creation and closed upon sending
        a final chunk. */
    bool isOpen() const {return isOpen_.load();}

    /** Obtains the ephemeral ID of this channel. */
    ChannelId id() const {return id_;}

    /** Sends a chunk to the other peer. */
    /** @returns
            - false if the associated Session object is destroyed
            - true if the chunk was accepted for processing
            - an error code if there was a problem processing the chunk
        @pre `this->isOpen() == true`
        @post `this->isOpen() == !chunk.isFinal()`
        @throws error::Logic if the preconditions were not met. */
    ErrorOrDone send(Chunk chunk)
    {
        CPPWAMP_LOGIC_CHECK(isOpen(), "wamp::Channel::send: Channel is closed");
        auto caller = caller_.lock();
        if (chunk.isFinal())
            isOpen_.store(false);
        if (!caller)
            return false;
        chunk.setCallInfo({}, uri_);
        return caller->sendCallerChunk(id_, std::move(chunk));
    }

    /** Thread-safe send. */
    std::future<ErrorOrDone> send(ThreadSafe, Chunk chunk)
    {
        CPPWAMP_LOGIC_CHECK(isOpen(), "wamp::Channel::send: Channel is closed");
        if (chunk.isFinal())
            isOpen_.store(false);
        auto caller = caller_.lock();
        if (!caller)
        {
            std::promise<ErrorOrDone> p;
            p.set_value(false);
            return p.get_future();
        }
        chunk.setCallInfo({}, uri_);
        return caller->safeSendCallerChunk(id_, std::move(chunk));
    }

    /** Sends an empty chunk marked as final. */
    void close()
    {
        if (isOpen())
            send(CallerChunk{true});
    }

    /** Thread-safe close. */
    void close(ThreadSafe)
    {
        if (isOpen())
            send(threadSafe, Chunk{true});
    }

private:
    using CallerPtr = std::weak_ptr<internal::Caller>;

    Error error_;
    Uri uri_;
    AnyReusableHandler<void (ErrorOr<Chunk>)> chunkHandler_;
    CallerPtr caller_;
    ChannelId id_ = nullId();
    std::atomic<bool> isOpen_;

public:
    // Internal use only
    CallerChannel(internal::PassKey, CallerPtr caller, Uri uri, RequestId id,
                  AnyReusableHandler<void (ErrorOr<Chunk>)> chunkHandler)
        : uri_(std::move(uri)),
          chunkHandler_(std::move(chunkHandler)),
          caller_(std::move(caller)),
          id_(id),
          isOpen_(true)
    {}

    void setError(internal::PassKey, Error error) {error_ = std::move(error);}
};

//------------------------------------------------------------------------------
/** Provides the interface for a caller to stream chunks of data. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeChannel
    : public std::enable_shared_from_this<CalleeChannel>
{
public:
    using Ptr = std::shared_ptr<CalleeChannel>;
    using Chunk = CalleeChunk;

    ~CalleeChannel() {close(threadSafe);}

    /** Returns true if the channel is open. */
    explicit operator bool() const {return isOpen();}

    /** Determines if the channel is open.
        This channel is open upon creation and closed upon sending
        an error or a final chunk. */
    bool isOpen() const {return isOpen_.load();}

    /** Obtains the ephemeral ID of this channel. */
    ChannelId id() const {return id_;}

    /** Sends a chunk to the other peer. */
    /** @returns
            - false if the associated Session object is destroyed
            - true if the chunk was accepted for processing
            - an error code if there was a problem processing the chunk
        @pre `this->isClosed() == false`
        @throws error::Logic if the preconditions were not met. */
    ErrorOrDone send(Chunk chunk)
    {
        CPPWAMP_LOGIC_CHECK(isOpen(),
                            "wamp::CallerChannel::send: Channel is closed");
        if (chunk.isFinal())
            isOpen_.store(false);
        auto caller = callee_.lock();
        if (!caller)
            return false;
        return caller->sendCalleeChunk(id_, std::move(chunk));
    }

    /** Thread-safe send. */
    std::future<ErrorOrDone> send(ThreadSafe, Chunk chunk)
    {
        CPPWAMP_LOGIC_CHECK(isOpen(),
                            "wamp::CallerChannel::send: Channel is closed");
        auto caller = callee_.lock();
        if (!caller)
        {
            std::promise<ErrorOrDone> p;
            p.set_value(false);
            return p.get_future();
        }
        return caller->safeSendCalleeChunk(id_, std::move(chunk));
    }

    /** Sends an Error to the other peer and closes the stream. */
    /** @returns
            - false if the associated Session object is destroyed
            - true if the error was accepted for processing
            - an error code if there was a problem processing the error
        @pre `this->isOpen() == true`
        @post `this->isOpen() == !chunk.isFinal()`
        @throws error::Logic if the preconditions were not met. */
    ErrorOrDone send(Error error)
    {
        CPPWAMP_LOGIC_CHECK(isOpen(),
                            "wamp::CalleeChannel::send: Channel is closed");
        isOpen_.store(false);
        auto caller = callee_.lock();
        if (!caller)
            return false;
        return caller->yield(id_, std::move(error));
    }

    /** Thread-safe send error. */
    std::future<ErrorOrDone> send(ThreadSafe, Error error)
    {
        CPPWAMP_LOGIC_CHECK(isOpen(),
                            "wamp::CalleeChannel::send: Channel is closed");
        auto caller = callee_.lock();
        if (!caller)
        {
            std::promise<ErrorOrDone> p;
            p.set_value(false);
            return p.get_future();
        }
        return caller->safeYield(id_, std::move(error));
    }

    /** Sends an empty chunk marked as final. */
    void close()
    {
        if (isOpen())
            send(Chunk{true});
    }

    /** Thread-safe close. */
    void close(ThreadSafe)
    {
        if (isOpen())
            send(threadSafe, CalleeChunk{true});
    }

private:
    using CalleePtr = std::weak_ptr<internal::Callee>;

    Uri uri_;
    AnyReusableHandler<void (Chunk)> chunkHandler_;
    CalleePtr callee_;
    ChannelId id_ = nullId();
    std::atomic<bool> isOpen_;

public:
    // Internal use only
    CalleeChannel(internal::PassKey, CalleePtr callee, Uri uri, RequestId id,
                  AnyReusableHandler<void (Chunk)> chunkHandler)
        : uri_(std::move(uri)),
          chunkHandler_(std::move(chunkHandler)),
          callee_(std::move(callee)),
          id_(id),
          isOpen_(true)
    {}
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/streaming.ipp"
#endif

#endif // CPPWAMP_STREAMING_HPP
