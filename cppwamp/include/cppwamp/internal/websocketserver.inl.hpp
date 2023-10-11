/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocketserver.hpp"
#include "websocketlistener.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Websocket>::Listener(
    AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c, RouterLogger::Ptr l)
    : Listening(s.label()),
    impl_(std::make_shared<internal::WebsocketListener>(
            std::move(e), std::move(i), std::move(s), std::move(c),
            std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Websocket>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::observe(Handler handler)
{
    impl_->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::establish()
{
    impl_->establish();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Websocket>::cancel() {impl_->cancel();}

} // namespace wamp
