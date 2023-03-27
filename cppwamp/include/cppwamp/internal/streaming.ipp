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

CPPWAMP_INLINE CallerInputChunk::CallerInputChunk()
    : Base(true, 0, Object{})
{}

CPPWAMP_INLINE CallerInputChunk::CallerInputChunk(internal::PassKey,
                                                  internal::Message&& msg)
    : Base(std::move(msg))
{}


//******************************************************************************
// CallerOutputChunk
//******************************************************************************

/** This sets the `CALL.Options.progress|bool` option accordingly. */
CPPWAMP_INLINE CallerOutputChunk::CallerOutputChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
)
    : Base(isFinal, 0, Object{}, String{})
{}

CPPWAMP_INLINE void CallerOutputChunk::setCallInfo(internal::PassKey, Uri uri)
{
    message().at(uriPos_) = std::move(uri);
}


//******************************************************************************
// CalleeInputChunk
//******************************************************************************

CPPWAMP_INLINE CalleeInputChunk::CalleeInputChunk()
    : Base(true, 0, 0, Object{})
{}

CPPWAMP_INLINE CalleeInputChunk::CalleeInputChunk(internal::PassKey,
                                                  Invocation&& inv)
    : Base(std::move(inv.message({})))
{}

CPPWAMP_INLINE StreamMode CalleeInputChunk::mode(internal::PassKey)
{
    bool wantsProgress = optionOr<bool>("receive_progress", false);

    using M = StreamMode;
    if (isFinal())
        return wantsProgress ? M::calleeToCaller : M::simpleCall;
    return wantsProgress ? M::bidirectional : M::callerToCallee;
}


//******************************************************************************
// CalleeOutputChunk
//******************************************************************************

CPPWAMP_INLINE CalleeOutputChunk::CalleeOutputChunk(
    bool isFinal ///< Marks this chunk as the final one in the stream
    )
    : Base(isFinal, 0, Object{})
{}

} // namespace wamp
