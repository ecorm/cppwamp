/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../streaming.hpp"

namespace wamp
{

//******************************************************************************
// CallerChunk
//******************************************************************************

/** This sets the `CALL.Options.progress|bool` option accordingly. */
CPPWAMP_INLINE CallerChunk::CallerChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
)
    : Base(String{}),
      isFinal_(isFinal)
{
    if (!isFinal_)
        withOption("progress", true);
}

CPPWAMP_INLINE bool CallerChunk::isFinal() const {return isFinal_;}

CPPWAMP_INLINE CallerChunk::CallerChunk(internal::PassKey,
                                        internal::CallMessage&& msg)
    : Base(std::move(msg)),
      isFinal_(!optionOr<bool>("progress", false))
{}

CPPWAMP_INLINE void CallerChunk::setCallInfo(internal::PassKey, String uri)
{
    message().setUri(std::move(uri));
}

CPPWAMP_INLINE internal::CallMessage&
CallerChunk::callMessage(internal::PassKey, RequestId reqId)
{
    message().setRequestId(reqId);
    return message();
}


//******************************************************************************
// CalleeChunk
//******************************************************************************

/** This sets the `RESULT.Options.progress|bool` option accordingly. */
CPPWAMP_INLINE CalleeChunk::CalleeChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
    )
    : isFinal_(isFinal)
{
    if (!isFinal_)
        withOption("progress", true);
}

CPPWAMP_INLINE bool CalleeChunk::isFinal() const {return isFinal_;}

CPPWAMP_INLINE CalleeChunk::CalleeChunk(internal::PassKey,
                                        internal::ResultMessage&& msg)
    : Base(std::move(msg)),
      isFinal_(!optionOr<bool>("progress", false))
{}

CPPWAMP_INLINE internal::ResultMessage&
CalleeChunk::resultMessage(internal::PassKey, RequestId reqId)
{
    message().setRequestId(reqId);
    return message();
}

CPPWAMP_INLINE internal::YieldMessage&
CalleeChunk::yieldMessage(internal::PassKey, RequestId reqId)
{
    message().setRequestId(reqId);
    return message().transformToYield();
}

} // namespace wamp
