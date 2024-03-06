/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tlsclient.hpp"
#include "tlsconnector.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tls>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(std::make_shared<internal::TlsConnector>(
        std::move(i), std::move(s), codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tls>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tls>::establish(Handler handler)
{
    impl_->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tls>::cancel() {impl_->cancel();}

} // namespace wamp
