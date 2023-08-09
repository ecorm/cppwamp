/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocket.hpp"
#include "websocketconnector.hpp"
#include "websocketlistener.hpp"

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

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Websocket> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct WebsocketListenerImpl
{
    WebsocketListenerImpl(IoStrand i, WebsocketEndpoint s, std::set<int> codecIds)
        : lstn(WebsocketListener::create(std::move(i), std::move(s),
                                         std::move(codecIds)))
    {}

    WebsocketListener::Ptr lstn;
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
CPPWAMP_INLINE void Connector<Websocket>::establish(Handler&& handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Websocket>::cancel() {impl_->cnct->cancel();}


//******************************************************************************
// Listener<Websocket>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Websocket>::Listener(IoStrand i, Settings s,
                                       std::set<int> codecIds)
    : Listening(s.label()),
      impl_(new internal::WebsocketListenerImpl(std::move(i), std::move(s),
                                          std::move(codecIds)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Websocket>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::establish(Handler&& handler)
{
    impl_->lstn->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
