/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPSERVER_HPP
#define CPPWAMP_TRANSPORTS_HTTPSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing HTTP transports. */
//------------------------------------------------------------------------------

#include <memory>
#include <set>
#include <utility>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "httpprotocol.hpp"

// TODO: HTTPS

namespace wamp
{

// Forward declarations
namespace internal
{
struct HttpConnectorImpl;
struct HttpListenerImpl;
}

//------------------------------------------------------------------------------
/** Listener specialization that implememts an HTTP server.
    Users do not  to use this class directly and should instead pass
    wamp::HttpEndpoint to wamp::Router::openServer via wamp::ServerOptions. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Http> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = HttpEndpoint;

    /** Constructor. */
    Listener(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c,
             const std::string& server = {}, RouterLogger::Ptr l = {});

    /** Destructor. */
    ~Listener() override;

    void observe(Handler handler) override;

    void establish() override;

    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Listener(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    /// @}

private:
    std::unique_ptr<internal::HttpListenerImpl> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPSERVER_HPP
