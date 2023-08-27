/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/uds.hpp"
#include "rawsockconnector.hpp"
#include "rawsocklistener.hpp"
#include "udsacceptor.hpp"
#include "udsopener.hpp"

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
    using RawsockOpener = internal::RawsockConnector<internal::UdsOpener>;

    UdsConnectorImpl(IoStrand i, UdsPath s, int codecId)
        : cnct(RawsockOpener::create(std::move(i), std::move(s), codecId))
    {}

    RawsockOpener::Ptr cnct;
};

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Uds> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct UdsListenerImpl
{
    using RawsockListener = internal::RawsockListener<internal::UdsAcceptor>;

    UdsListenerImpl(AnyIoExecutor e, IoStrand i, UdsPath s,
                    std::set<int> codecIds)
        : lstn(RawsockListener::create(std::move(e), std::move(i), std::move(s),
                                       std::move(codecIds)))
    {}

    RawsockListener::Ptr lstn;
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
                                       std::set<int> codecIds)
    : Listening(s.label()),
      impl_(new internal::UdsListenerImpl(std::move(e), std::move(i),
                                          std::move(s), std::move(codecIds)))
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
