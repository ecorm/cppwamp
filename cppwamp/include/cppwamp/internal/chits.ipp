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
CPPWAMP_INLINE ErrorOrDone CallChit::cancel() const
{
    return cancel(cancelMode_);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone> CallChit::cancel(ThreadSafe) const
{
    return cancel(threadSafe, cancelMode_);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone CallChit::cancel(CallCancelMode mode) const
{
    auto caller = caller_.lock();
    if (caller)
        return caller->cancelCall(reqId_, mode);
    return false;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone>
CallChit::cancel(ThreadSafe, CallCancelMode mode) const
{
    auto caller = caller_.lock();
    if (caller)
        return caller->safeCancelCall(reqId_, mode);
    return futureValue(false);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::future<ErrorOrDone> CallChit::futureValue(bool value)
{
    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(value);
    return f;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CallChit::CallChit(CallerPtr caller, RequestId reqId,
                                  CallCancelMode mode, bool progressive,
                                  internal::PassKey)
    : caller_(caller),
      reqId_(reqId),
      cancelMode_(mode)
{}

} // namespace wamp
