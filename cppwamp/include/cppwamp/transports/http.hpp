/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTP_HPP
#define CPPWAMP_TRANSPORTS_HTTP_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing HTTP transports. */
//------------------------------------------------------------------------------

#include <memory>
#include <set>
#include <utility>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../connector.hpp"
#include "../listener.hpp"
#include "httpendpoint.hpp"

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
/** Listener specialization that establishes a server-side HTTP transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Http> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = HttpEndpoint;

    /** Collection type used for codec IDs. */
    using CodecIds = std::set<int>;

    /** Constructor. */
    Listener(IoStrand i, Settings s, CodecIds codecIds);

    /** Destructor. */
    ~Listener() override;

    void establish(Handler&& handler) override;

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
#include "../internal/http.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTP_HPP
