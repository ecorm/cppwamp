/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocketclient.hpp"
#include "websocketconnector.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Connector<Websocket> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct WebsocketConnectorImpl
{
    WebsocketConnectorImpl(IoStrand i, WebsocketHost s, int codecId)
        : cnct(WebsocketConnector::create(std::move(i), std::move(s), codecId))
    {}

    WebsocketConnector::Ptr cnct;
};

} // namespace internal


//******************************************************************************
// Connector<Websocket>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Websocket>::Connector(IoStrand i, Settings s,
                                               int codecId)
    : impl_(new internal::WebsocketConnectorImpl(std::move(i), std::move(s),
                                                 codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Websocket>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Websocket>::establish(Handler handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Websocket>::cancel() {impl_->cnct->cancel();}

} // namespace wamp
