/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/udsclient.hpp"
#include "udsconnector.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Uds>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(std::make_shared<internal::UdsConnector>(
        std::move(i), std::move(s), codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Uds>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Uds>::establish(Handler handler)
{
    impl_->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Uds>::cancel() {impl_->cancel();}

} // namespace wamp
