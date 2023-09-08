/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/uds.hpp"
#include "udsconnector.hpp"
#include "udslistener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Connector<Uds> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct UdsConnectorImpl
{
    using ConnectorType = internal::UdsConnector;

    UdsConnectorImpl(IoStrand i, UdsHost s, int codecId)
        : cnct(ConnectorType::create(std::move(i), std::move(s), codecId))
    {}

    ConnectorType::Ptr cnct;
};

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Uds> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct UdsListenerImpl
{
    using ListenerType = internal::UdsListener;

    UdsListenerImpl(AnyIoExecutor e, IoStrand i, UdsEndpoint s, CodecIdSet c)
        : lstn(ListenerType::create(std::move(e), std::move(i), std::move(s),
                                    std::move(c)))
    {}

    ListenerType::Ptr lstn;
};

} // namespace internal


//******************************************************************************
// Connector<Uds>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Uds>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(new internal::UdsConnectorImpl(std::move(i), std::move(s), codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Uds>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Uds>::establish(Handler handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Uds>::cancel() {impl_->cnct->cancel();}


//******************************************************************************
// Listener<Uds>
//******************************************************************************

CPPWAMP_INLINE Listener<Uds>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                       CodecIdSet c)
    : Listening(s.label()),
      impl_(new internal::UdsListenerImpl(std::move(e), std::move(i),
                                          std::move(s), std::move(c)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Uds>::Listener(Listener&&) noexcept = default;

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Uds>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Uds>& Listener<Uds>::operator=(Listener&&) noexcept
    = default;

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
