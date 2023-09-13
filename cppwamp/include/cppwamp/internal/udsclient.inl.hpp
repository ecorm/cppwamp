/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/udsclient.hpp"
#include "udsconnector.hpp"

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

} // namespace wamp
