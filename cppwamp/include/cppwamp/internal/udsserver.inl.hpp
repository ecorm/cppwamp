/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/udsserver.hpp"
#include "udslistener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Uds> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct UdsListenerImpl
{
    using ListenerType = internal::UdsListener;

    UdsListenerImpl(AnyIoExecutor e, IoStrand i, UdsEndpoint s, CodecIdSet c,
                    const std::string& server, RouterLogger::Ptr l)
        : lstn(ListenerType::create(std::move(e), std::move(i), std::move(s),
                                    std::move(c), server, std::move(l)))
    {}

    ListenerType::Ptr lstn;
};

} // namespace internal


//******************************************************************************
// Listener<Uds>
//******************************************************************************

CPPWAMP_INLINE Listener<Uds>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                       CodecIdSet c, const std::string& server,
                                       RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(new internal::UdsListenerImpl(
          std::move(e), std::move(i), std::move(s), std::move(c), server,
          std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Uds>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::observe(Handler handler)
{
    impl_->lstn->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::establish() {impl_->lstn->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
