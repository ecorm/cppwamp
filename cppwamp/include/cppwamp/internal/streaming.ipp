/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../streaming.hpp"

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
// CalleeInputChunk
//******************************************************************************

CPPWAMP_INLINE CalleeInputChunk::CalleeInputChunk() {}

CPPWAMP_INLINE CalleeInputChunk::CalleeInputChunk(
    internal::PassKey, internal::InvocationMessage&& msg)
    : Base(std::move(msg))
{}


//******************************************************************************
// CalleeOutputChunk
//******************************************************************************

/** This sets the `RESULT.Options.progress|bool` option accordingly. */
CPPWAMP_INLINE CalleeOutputChunk::CalleeOutputChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
    )
    : Base(isFinal)
{}

CPPWAMP_INLINE internal::YieldMessage&
CalleeOutputChunk::yieldMessage(internal::PassKey, RequestId reqId)
{
    message().setRequestId(reqId);
    return message();
}

} // namespace wamp
