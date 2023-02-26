/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../streaming.hpp"

namespace wamp
{

//******************************************************************************
// OutputChunk
//******************************************************************************

/** This sets the `CALL.Options.progress|bool` option accordingly. */
CPPWAMP_INLINE OutputChunk::OutputChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
)
    : Base(String{}),
      isFinal_(isFinal)
{
    if (!isFinal_)
        withOption("progress", true);
}

CPPWAMP_INLINE bool OutputChunk::isFinal() const {return isFinal_;}

CPPWAMP_INLINE void OutputChunk::setCallInfo(
    internal::PassKey, RequestId reqId, String uri)
{
    requestId_ = reqId;
    message().setRequestId(reqId);
    message().setUri(std::move(uri));
}

CPPWAMP_INLINE RequestId OutputChunk::requestId(internal::PassKey) const
{
    return requestId_;
}

CPPWAMP_INLINE internal::CallMessage&
OutputChunk::callMessage(internal::PassKey)
{
    message().setRequestId(requestId_);
    return message();
}

} // namespace wamp
