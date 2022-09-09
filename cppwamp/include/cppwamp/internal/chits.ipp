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
CPPWAMP_INLINE CallChit::CallChit(CallerPtr caller, RequestId reqId,
                                  CallCancelMode mode, internal::PassKey)
    : caller_(caller),
      reqId_(reqId),
      cancelMode_(mode)
{}

} // namespace wamp
