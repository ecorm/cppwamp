/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../cancellation.hpp"
#include "../api.hpp"
#include "clientcontext.hpp"

namespace wamp
{

//******************************************************************************
// CallCancellationSlot::Impl
//******************************************************************************

struct CallCancellationSlot::Impl
{
    using Ptr = std::shared_ptr<Impl>;
    using WeakPtr = std::weak_ptr<Impl>;

    Handler handler;
};


//******************************************************************************
// CallCancellationSlot::Handler
//******************************************************************************

CPPWAMP_INLINE CallCancellationSlot::Handler::Handler() {}

CPPWAMP_INLINE CallCancellationSlot::Handler::Handler(
    internal::ClientContext caller, RequestId requestId)
    : caller_(std::move(caller)),
      requestId_(requestId)
{}

CPPWAMP_INLINE CallCancellationSlot::Handler::operator bool()
{
    return requestId_ != nullId();
}

CPPWAMP_INLINE void
CallCancellationSlot::Handler::operator()(CallCancelMode cancelMode)
{
    caller_.safeCancelCall(requestId_, cancelMode);
}


//******************************************************************************
// CallCancellationSlot
//******************************************************************************

CPPWAMP_INLINE CallCancellationSlot::CallCancellationSlot() {}

CPPWAMP_INLINE CallCancellationSlot::Handler&
CallCancellationSlot::assign(Handler f)
{
    impl_->handler = std::move(f);
    return impl_->handler;
}

CPPWAMP_INLINE CallCancellationSlot::Handler&
CallCancellationSlot::emplace(internal::ClientContext caller, RequestId reqId)
{
    impl_->handler = Handler{std::move(caller), reqId};
    return impl_->handler;
}

CPPWAMP_INLINE void CallCancellationSlot::clear() {impl_->handler = {};}

CPPWAMP_INLINE bool CallCancellationSlot::has_handler() const
{
    return bool(impl_->handler);
}

CPPWAMP_INLINE bool CallCancellationSlot::is_connected() const
{
    return impl_ != nullptr;
}

CPPWAMP_INLINE bool
CallCancellationSlot::operator==(const CallCancellationSlot& rhs) const
{
    return impl_ == rhs.impl_;
}

CPPWAMP_INLINE bool
CallCancellationSlot::operator!=(const CallCancellationSlot& rhs) const
{
    return impl_ != rhs.impl_;
}


//******************************************************************************
// CallCancellationSignal
//******************************************************************************

CPPWAMP_INLINE CallCancellationSignal::CallCancellationSignal()
    : slotImpl_(new CallCancellationSlot::Impl)
{}

CPPWAMP_INLINE void CallCancellationSignal::emit(CallCancelMode cancelMode)
{
    slotImpl_->handler(cancelMode);
}

CPPWAMP_INLINE CallCancellationSlot CallCancellationSignal::slot() const
{
    return CallCancellationSlot{slotImpl_};
}

} // namespace wamp
