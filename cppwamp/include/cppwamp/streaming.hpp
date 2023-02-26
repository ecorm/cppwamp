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

#include <memory>
#include "api.hpp"
#include "payload.hpp"
#include "tagtypes.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "internal/passkey.hpp"
#include "internal/wampmessage.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class Caller; }

//------------------------------------------------------------------------------
enum class Direction
{
    in,
    out,
    bidi
};

//------------------------------------------------------------------------------
/** Contains signalling information used to establish a channel for streaming
    with a remote peer.
    This object is used to generate an initiating `CALL` message configured for
    [Progressive Calls][1] and/or [Progressive Call Results][2].
    [1]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-calls)
    [2]: (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-call-results) */
//------------------------------------------------------------------------------
class CPPWAMP_API Invitation : public Payload<Invitation, internal::CallMessage>
{
public:
    /** Constructor. */
    explicit Invitation(
        String uri, ///< The URI with which to associate this invitation.
        Direction streamDir ///< The desired stream direction(s)
        )
        : Base(std::move(uri), makeOptions(streamDir)),
        direction_(streamDir)
    {}

    Direction streamDirection() const {return direction_;}

private:
    using Base = Payload<Invitation, internal::CallMessage>;

    static Object makeOptions(Direction dir)
    {
        static const Object in{{"progressive_call_results", true}};
        static const Object out{{"progress", true}};
        static const Object bidi{{"progress", true},
                                 {"progressive_call_results", true}};
        switch (dir)
        {
        case Direction::in:   return in;
        case Direction::out:  return out;
        case Direction::bidi: return bidi;
        default: assert(false && "Unexpected Direction enumerator");
        }
        return {};
    }

    Direction direction_;

public:
    // Internal use only
    internal::CallMessage& callMessage(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Contains the payload arguments of a chunk to be streamed via a progressive
    `CALL` message.
    See [Progressive Calls in the WAMP Specification]
    (https://wamp-proto.org/wamp_latest_ietf.html#name-progressive-calls) */
//------------------------------------------------------------------------------
class CPPWAMP_API OutputChunk : public Payload<OutputChunk,
                                               internal::CallMessage>
{
public:
    /** Constructor. */
    explicit OutputChunk(bool isFinal = false);

    /** Indicates if the chunk is the final one. */
    bool isFinal() const;

private:
    using Base = Payload<OutputChunk, internal::CallMessage>;

    /** Disallow the setting of options. */
    using Base::withOption;

    /** Disallow the setting of options. */
    using Base::withOptions;

    RequestId requestId_ = nullId();
    bool isFinal_ = false;

public:
    // Internal use only
    void setCallInfo(internal::PassKey, RequestId reqId, String uri);
    RequestId requestId(internal::PassKey) const;
    internal::CallMessage& callMessage(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Provides the interface for streaming chunks of data to another peer.
    This is a lightweight object emitted by Session::invite. */
//------------------------------------------------------------------------------
class CPPWAMP_API Channel
{
public:
    /** Constructs an empty registration. */
    Channel();

    /** Copy constructor. */
    Channel(const Channel& other);

    /** Move constructor. */
    Channel(Channel&& other) noexcept;

    /** Returns false if the channel is closed. */
    explicit operator bool() const;

    /** Copy assignment. */
    Channel& operator=(const Channel& other);

    /** Move assignment. */
    Channel& operator=(Channel&& other) noexcept;

    /** Sends a chunk to the other peer. */
    void send(OutputChunk chunk, bool final = false);

    /** Sends an empty chunk marked as final. */
    void close() const;

    /** Thread-safe close. */
    void close(ThreadSafe) const;

private:
    using CallerPtr = std::weak_ptr<internal::Caller>;

    CallerPtr caller_;
    RequestId id_ = nullId();

public:
    // Internal use only
    Channel(internal::PassKey, CallerPtr caller);
};


//------------------------------------------------------------------------------
/** Limits a Channel's lifetime to a particular scope.
    @see Channel */
//------------------------------------------------------------------------------
class CPPWAMP_API ScopedChannel : public Channel
{
public:
    /** Default constructs an empty ScopedRegistration. */
    ScopedChannel();

    /** Move constructor. */
    ScopedChannel(ScopedChannel&& other) noexcept;

    /** Converting constructor taking a Channel object to manage. */
    ScopedChannel(Channel channel);

    /** Destructor which automatically closes the channel. */
    ~ScopedChannel();

    /** Move assignment. */
    ScopedChannel& operator=(ScopedChannel&& other) noexcept;

    /** Assigns another Channel to manage.
        The old registration is automatically unregistered. */
    ScopedChannel& operator=(Channel channel);

    /** Releases the registration so that it will no longer be automatically
        unregistered if the ScopedRegistration is destroyed or reassigned. */
    void release();

    /** Non-copyable. */
    ScopedChannel(const ScopedChannel&) = delete;

    /** Non-copyable. */
    ScopedChannel& operator=(const ScopedChannel&) = delete;

private:
    using Base = Channel;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/streaming.ipp"
#endif

#endif // CPPWAMP_STREAMING_HPP
