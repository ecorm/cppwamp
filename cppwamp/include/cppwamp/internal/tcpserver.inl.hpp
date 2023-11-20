/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tcpserver.hpp"
#include "tcplistener.hpp"

namespace wamp
{

//******************************************************************************
// Listener<Tcp>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                       CodecIdSet c, RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(std::make_shared<internal::TcpListener>(
          std::move(e), std::move(i), std::move(s), std::move(c),
          std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::observe(Handler handler)
{
    impl_->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::establish() {impl_->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Transporting::Ptr Listener<Tcp>::take() {return impl_->take();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::drop() {impl_->drop();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::cancel() {impl_->cancel();}

} // namespace wamp
