/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocketserver.hpp"
#include "websocketlistener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Websocket> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct WebsocketListenerImpl
{
    WebsocketListenerImpl(AnyIoExecutor e, IoStrand i, WebsocketEndpoint s,
                          CodecIdSet c, const std::string& server,
                          RouterLogger::Ptr l)
        : lstn(WebsocketListener::create(std::move(e), std::move(i),
                                         std::move(s), std::move(c),
                                         server, std::move(l)))
    {}

    WebsocketListener::Ptr lstn;
};

} // namespace internal


//******************************************************************************
// Listener<Websocket>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Websocket>::Listener(
    AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c,
    const std::string& server, RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(new internal::WebsocketListenerImpl(
            std::move(e), std::move(i), std::move(s), std::move(c), server,
            std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Websocket>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::observe(Handler handler)
{
    impl_->lstn->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::establish()
{
    impl_->lstn->establish();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
