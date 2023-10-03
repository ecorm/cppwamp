/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tcpserver.hpp"
#include "tcplistener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Tcp> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct TcpListenerImpl
{
    using ListenerType = internal::TcpListener;

    TcpListenerImpl(AnyIoExecutor e, IoStrand i, TcpEndpoint s, CodecIdSet c,
                    const std::string& server, RouterLogger::Ptr l)
        : lstn(ListenerType::create(std::move(e), std::move(i), std::move(s),
                                    std::move(c), server, std::move(l)))
    {}

    ListenerType::Ptr lstn;
};

} // namespace internal


//******************************************************************************
// Listener<Tcp>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                       CodecIdSet c, const std::string& server,
                                       RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(new internal::TcpListenerImpl(
          std::move(e), std::move(i), std::move(s), std::move(c), server,
          std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::observe(Handler handler)
{
    impl_->lstn->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::establish() {impl_->lstn->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
