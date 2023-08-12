/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPENDPOINT_HPP
#define CPPWAMP_TRANSPORTS_HTTPENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying HTTP server parameters and
           options. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include "../api.hpp"
#include "tcpprotocol.hpp"
#include "httpprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains HTTP host address information, as well as other socket options. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpEndpoint
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Http;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit HttpEndpoint(Port port);

    /** Constructor taking an address string and a port number. */
    HttpEndpoint(std::string address, unsigned short port);

    /** Specifies the underlying TCP socket options to use. */
    HttpEndpoint& withOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    HttpEndpoint& withMaxRxLength(std::size_t length);

    /** Obtains the endpoint address. */
    const std::string& address() const;

    /** Obtains the the port number. */
    Port port() const;

    /** Obtains the transport options. */
    const TcpOptions& options() const;

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const;

    /** Generates a human-friendly string of the HTTP address/port. */
    std::string label() const;

private:
    std::string address_;
    TcpOptions options_;
    std::size_t maxRxLength_ = 16*1024*1024;
    Port port_ = 0;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpendpoint.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPENDPOINT_HPP
