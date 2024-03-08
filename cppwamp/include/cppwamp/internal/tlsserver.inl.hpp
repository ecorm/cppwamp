/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tlsserver.hpp"
#include "tlslistener.hpp"

namespace wamp
{

//******************************************************************************
// Listener<Tls>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tls>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                       CodecIdSet c, RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(std::make_shared<internal::TlsListener>(
            std::move(e), std::move(i), std::move(s), std::move(c),
            std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tls>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tls>::observe(Handler handler)
{
    impl_->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tls>::establish() {impl_->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOr<Transporting::Ptr> Listener<Tls>::take()
{
    return impl_->take();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tls>::drop() {impl_->drop();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tls>::cancel() {impl_->cancel();}

} // namespace wamp
