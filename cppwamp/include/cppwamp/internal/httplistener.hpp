/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPLISTENER_HPP
#define CPPWAMP_INTERNAL_HTTPLISTENER_HPP

#include <cassert>
#include <memory>
#include <set>
#include <type_traits>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include "../asiodefs.hpp"
#include "../codec.hpp"
#include "../listener.hpp"
#include "../version.hpp"
#include "../transports/httpendpoint.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpListener : public std::enable_shared_from_this<HttpListener>
{
public:
    using Ptr       = std::shared_ptr<HttpListener>;
    using Settings  = HttpEndpoint;
    using Handler   = Listening::Handler;

    static Ptr create(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c)
    {
        return nullptr;
        // TODO
    }

    void observe(Handler handler)
    {
        // TODO
    }

    void establish()
    {
        // TODO
    }

    void cancel()
    {
        // TODO
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPLISTENER_HPP
