/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/http.hpp"
#include "httplistener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Http> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct HttpListenerImpl
{
    HttpListenerImpl(AnyIoExecutor e, IoStrand i, HttpEndpoint s, CodecIdSet c)
        : lstn(HttpListener::create(std::move(e), std::move(i), std::move(s),
                                    std::move(c)))
    {}

    HttpListener::Ptr lstn;
};

} // namespace internal


//******************************************************************************
// Listener<Http>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Http>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                        CodecIdSet c)
    : Listening(s.label()),
      impl_(new internal::HttpListenerImpl(std::move(e), std::move(i),
                                           std::move(s), std::move(c)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Http>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::observe(Handler handler)
{
    impl_->lstn->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::establish() {impl_->lstn->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
