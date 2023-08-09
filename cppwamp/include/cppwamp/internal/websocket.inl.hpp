/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocket.hpp"
#include "rawsockconnector.hpp"
#include "rawsocklistener.hpp"
//#include "tcpacceptor.hpp"
//#include "tcpopener.hpp"

namespace wamp
{

#if 0

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Connector<Websocket> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct WebsocketConnectorImpl
{
    using RawsockConnector = internal::RawsockConnector<internal::WebsocketOpener>;

    WebsocketConnectorImpl(IoStrand i, WebsocketHost s, int codecId)
        : cnct(RawsockConnector::create(std::move(i), std::move(s), codecId))
    {}

    RawsockConnector::Ptr cnct;
};

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Websocket> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct WebsocketListenerImpl
{
    using RawsockListener = internal::RawsockListener<internal::WebsocketAcceptor>;

    WebsocketListenerImpl(IoStrand i, WebsocketEndpoint s, std::set<int> codecIds)
        : lstn(RawsockListener::create(std::move(i), std::move(s),
                                       std::move(codecIds)))
    {}

    RawsockListener::Ptr lstn;
};

} // namespace internal


//******************************************************************************
// Connector<Websocket>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Websocket>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(new internal::WebsocketConnectorImpl(std::move(i), std::move(s), codecId))
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

#endif

} // namespace wamp
