/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/udsserver.hpp"
#include "udslistener.hpp"

namespace wamp
{

//******************************************************************************
// Listener<Uds>
//******************************************************************************

CPPWAMP_INLINE Listener<Uds>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                       CodecIdSet c, RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(std::make_shared<internal::UdsListener>(
          std::move(e), std::move(i), std::move(s), std::move(c), std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Uds>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::observe(Handler handler)
{
    impl_->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::establish() {impl_->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOr<Transporting::Ptr> Listener<Uds>::take()
{
    return impl_->take();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::drop() {impl_->drop();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::cancel() {impl_->cancel();}

} // namespace wamp
