/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tcpclient.hpp"
#include "tcpconnector.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(std::make_shared<internal::TcpConnector>(std::move(i), std::move(s),
                                                     codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::establish(Handler handler)
{
    impl_->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::cancel() {impl_->cancel();}

} // namespace wamp
