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
#include "rpcinfo.hpp"
#include "wampdefs.hpp"

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
    detached, ///< Not connected to an underlying shared channel
    awaiting, ///< Awaiting a response
    open,     ///< Ready to send chunks
    closed,   ///< Final chunk sent
    abandoned ///< Associated Session was closed/disconnected
};

//------------------------------------------------------------------------------
/** Consolidates common properties of streaming chunks. */
//------------------------------------------------------------------------------
template <typename TDerived, internal::MessageKind K>
class CPPWAMP_API Chunk : public Payload<TDerived, K>
{
public:
    /** Indicates if the chunk is the final one. */
    bool isFinal() const {return isFinal_;}

    /** Obtains the channel ID associated with the chunk. */
    ChannelId channelId() const {return this->message().requestId();}

private:
    using Base = Payload<TDerived, K>;

    bool isFinal_ = false;

protected:
    Chunk() = default;

    template <typename... Ts>
    explicit Chunk(bool isFinal, Ts&&... fields)
        : Base(in_place, std::forward<Ts>(fields)...),
          isFinal_(isFinal)
    {
        if (!this->isFinal())
            this->withOption("progress", true);
    }

    explicit Chunk(internal::Message&& msg)
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
class CPPWAMP_API CallerInputChunk
    : public Chunk<CallerInputChunk, internal::MessageKind::result>
{
public:
    /** Default constructor. */
    CallerInputChunk();

private:
    using Base = Chunk<CallerInputChunk, internal::MessageKind::result>;

public:
    // Internal use only
    CallerInputChunk(internal::PassKey, internal::Message&& msg);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk to be sent via a progressive
    `CALL` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallerOutputChunk
    : public Chunk<CallerOutputChunk, internal::MessageKind::call>
{
public:
    /** Constructor. */
    explicit CallerOutputChunk(bool isFinal = false);

private:
    static constexpr unsigned uriPos_ = 3;

    using Base = Chunk<CallerOutputChunk, internal::MessageKind::call>;

public:
    // Internal use only
    void setCallInfo(internal::PassKey, ChannelId channelId, Uri uri);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk received via a progressive
    `INVOCATION` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeInputChunk
    : public Chunk<CalleeInputChunk, internal::MessageKind::invocation>
{
public:
    /** Default constructor. */
    CalleeInputChunk();

private:
    static constexpr unsigned registrationIdPos_ = 2;

    using Base = Chunk<CalleeInputChunk, internal::MessageKind::invocation>;

public:
    // Internal use only
    CalleeInputChunk(internal::PassKey, Invocation&& inv);
    StreamMode mode(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Contains the payload of a chunk to be sent via a progressive
    `YIELD` message. */
//------------------------------------------------------------------------------
class CPPWAMP_API CalleeOutputChunk
    : public Chunk<CalleeOutputChunk, internal::MessageKind::yield>
{
public:
    /** Constructor. */
    explicit CalleeOutputChunk(bool isFinal = false);

private:
    using Base = Chunk<CalleeOutputChunk, internal::MessageKind::yield>;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/streaming.inl.hpp"
#endif

#endif // CPPWAMP_STREAMING_HPP
