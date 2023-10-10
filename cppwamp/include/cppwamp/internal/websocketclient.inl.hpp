/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocketclient.hpp"
#include "websocketconnector.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Websocket>::Connector(IoStrand i, Settings s,
                                               int codecId)
    : impl_(std::make_shared<internal::WebsocketConnector>(
        std::move(i), std::move(s), codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Websocket>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Websocket>::establish(Handler handler)
{
    impl_->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Websocket>::cancel() {impl_->cancel();}

} // namespace wamp
