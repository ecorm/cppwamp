/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpsserver.hpp"
#include "httpslistener.hpp"

namespace wamp
{

//******************************************************************************
// Listener<Https>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Https>::Listener(
    AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c, RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(std::make_shared<internal::HttpsListener>(
            std::move(e), std::move(i), std::move(s), std::move(c),
            std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Https>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Https>::observe(Handler handler)
{
    impl_->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Https>::establish() {impl_->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOr<Transporting::Ptr> Listener<Https>::take()
{
    return impl_->take();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Https>::drop() {impl_->drop();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Https>::cancel() {impl_->cancel();}

} // namespace wamp
