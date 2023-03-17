/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_STREAMING_HPP
#define CPPWAMP_STREAMING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains common streaming facilities. */
//------------------------------------------------------------------------------

#include "payload.hpp"
#include "wampdefs.hpp"
#include "internal/wampmessage.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/// Ephemeral ID associated with a streaming channel
//------------------------------------------------------------------------------
using ChannelId = RequestId;


//------------------------------------------------------------------------------
/// Enumerates the possible streaming modes.
//------------------------------------------------------------------------------
enum class StreamMode
{
    unknown,        ///< Stream mode unknown due to detached channel.
    simpleCall,     ///< No progressive calls results or invocations.
    calleeToCaller, ///< Progressive call results only.
    callerToCallee, ///< Progressive call invocations only.
    bidirectional   ///< Both progressive calls results and invocations
};

//------------------------------------------------------------------------------
/// Enumerates the possible caller channel states.
//------------------------------------------------------------------------------
enum class ChannelState
{
    detached, // Not connected to an underlying shared channel
    awaiting, // Awaiting a response
    open,     // Ready to send chunks
    closed    // Final chunk sent
};

//------------------------------------------------------------------------------
/** Consolidates common properties of streaming chunks. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TMessage>
class CPPWAMP_API Chunk : public Payload<TDerived, TMessage>
{
public:
    /** Indicates if the chunk is the final one. */
    bool isFinal() const {return isFinal_;}

    /** Obtains the channel ID associated with the chunk. */
    ChannelId channelId() const {return this->message().requestId();}

private:
    using Base = Payload<TDerived, TMessage>;

    bool isFinal_ = false;

protected:
    Chunk() = default;

    explicit Chunk(bool isFinal)
        : isFinal_(isFinal)
    {
        if (!this->isFinal())
            withOption("progress", true);
    }

    explicit Chunk(TMessage&& msg)
        : Base(std::move(msg))
    {
        isFinal_ = !this->template optionOr<bool>("progress", false);
    }

    // Disallow the user setting options.
    using Base::withOption;
    using Base::withOptions;
};

//------------------------------------------------------------------------------
/** Contains the payload of a chunk received via a progressive
    `RESULT` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerInputChunk : public Chunk<CallerInputChunk,
                                                  internal::ResultMessage>
{
public:
    /** Default constructor. */
    CallerInputChunk();

private:
    using Base = Chunk<CallerInputChunk, internal::ResultMessage>;

public:
    // Internal use only
    CallerInputChunk(internal::PassKey, internal::ResultMessage&& msg);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk to be sent via a progressive
    `CALL` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerOutputChunk : public Chunk<CallerOutputChunk,
                                                   internal::CallMessage>
{
public:
    /** Constructor. */
    explicit CallerOutputChunk(bool isFinal = false);

private:
    using Base = Chunk<CallerOutputChunk, internal::CallMessage>;

public:
    // Internal use only
    void setCallInfo(internal::PassKey, Uri uri);
    internal::CallMessage& callMessage(internal::PassKey, RequestId reqId);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk received via a progressive
    `INVOCATION` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeInputChunk : public Chunk<CalleeInputChunk,
                                                  internal::InvocationMessage>
{
public:
    /** Default constructor. */
    CalleeInputChunk();

private:
    using Base = Chunk<CalleeInputChunk, internal::InvocationMessage>;

public:
    // Internal use only
    CalleeInputChunk(internal::PassKey, internal::InvocationMessage&& msg);
    StreamMode mode(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk to be sent via a progressive
    `YIELD` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeOutputChunk : public Chunk<CalleeOutputChunk,
                                                   internal::YieldMessage>
{
public:
    /** Constructor. */
    explicit CalleeOutputChunk(bool isFinal = false);

private:
    using Base = Chunk<CalleeOutputChunk, internal::YieldMessage>;

public:
    // Internal use only
    internal::YieldMessage& yieldMessage(internal::PassKey, RequestId reqId);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/streaming.ipp"
#endif

#endif // CPPWAMP_STREAMING_HPP
