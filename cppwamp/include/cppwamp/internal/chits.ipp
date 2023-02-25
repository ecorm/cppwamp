/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../chits.hpp"
#include "caller.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE CallChit::CallChit() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CallChit::operator bool() const {return reqId_ != invalidId_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE RequestId CallChit::requestId() const {return reqId_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CallCancelMode CallChit::cancelMode() const {return cancelMode_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool CallChit::isProgressive() const {return isProgressive_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool CallChit::finalChunkSent() const {return finalChunkSent_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CallChit::cancel() const
{
    cancel(cancelMode_);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CallChit::cancel(ThreadSafe) const
{
    cancel(threadSafe, cancelMode_);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CallChit::cancel(CallCancelMode mode) const
{
    auto caller = caller_.lock();
    if (caller)
        caller->cancelCall(reqId_, mode);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CallChit::cancel(ThreadSafe, CallCancelMode mode) const
{
    auto caller = caller_.lock();
    if (caller)
        caller->safeCancelCall(reqId_, mode);
}

//------------------------------------------------------------------------------
/** @pre this->isProgressive()
    @pre !this->finalChunkSent()
    @throws error::Logic if the precondition is not met. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone CallChit::send(OutputChunk chunk) const
{
    // TODO: Test CallChit::send
    CPPWAMP_LOGIC_CHECK(
        isProgressive_,
        "wamp::CallChit::send: initial call was not progressive");
    CPPWAMP_LOGIC_CHECK(
        !finalChunkSent_,
        "wamp::CallChit::send: final chunk was already sent");
    chunk.setRequestId({}, reqId_);
    auto caller = caller_.lock();
    if (caller)
        return caller->sendChunk(std::move(chunk));
    return false;
}

//------------------------------------------------------------------------------
/** @pre this->isProgressive()
    @pre !this->finalChunkSent()
    @throws error::Logic if the precondition is not met. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone>
CallChit::send(ThreadSafe, OutputChunk chunk) const
{
    CPPWAMP_LOGIC_CHECK(
        isProgressive_,
        "wamp::CallChit::send: initial call was not progressive");
    CPPWAMP_LOGIC_CHECK(
        !finalChunkSent_,
        "wamp::CallChit::send: final chunk was already sent");
    chunk.setRequestId({}, reqId_);
    auto caller = caller_.lock();
    if (caller)
        return caller->safeSendChunk(std::move(chunk));
    std::promise<ErrorOrDone> p;
    p.set_value(ErrorOrDone{false});
    return p.get_future();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CallChit::CallChit(CallerPtr caller, RequestId reqId,
                                  CallCancelMode mode, bool progressive,
                                  internal::PassKey)
    : caller_(caller),
      reqId_(reqId),
      cancelMode_(mode),
      isProgressive_(progressive)
{}

} // namespace wamp
