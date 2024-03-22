/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPSPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_HTTPSPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic HTTPS protocol definitions. */
//------------------------------------------------------------------------------

#include "httpprotocol.hpp"
#include "sslcontext.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the HTTPS transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Https
{
    constexpr Https() = default;
};


//------------------------------------------------------------------------------
/** Contains HTTP server address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpsEndpoint
    : public SocketEndpoint<HttpsEndpoint, Https, TcpOptions,
                            HttpListenerLimits>
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Https;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit HttpsEndpoint(Port port, SslContextGenerator generator);

    /** Constructor taking an address string and port number. */
    HttpsEndpoint(std::string address, Port port,
                  SslContextGenerator generator);

    /** Specifies the default server block options. */
    HttpsEndpoint& withOptions(HttpServerOptions options);

    /** Adds a server block. */
    HttpsEndpoint& addBlock(HttpServerBlock block);

    /** Obtains the endpoint-level server options. */
    const HttpServerOptions& options() const;

    /** Accesses the endpoint-level server options. */
    HttpServerOptions& options();

    /** Finds the best-matching server block for the given host name. */
    HttpServerBlock* findBlock(std::string hostName);

    /** Generates a human-friendly string of the HTTP address/port. */
    std::string label() const;

private:
    using Base = SocketEndpoint<HttpsEndpoint, Https, TcpOptions,
                                HttpListenerLimits>;

    internal::HttpServerBlockMap serverBlocks_;
    HttpServerOptions options_;
    SslContextGenerator sslContextGenerator_;

public: // Internal use only
    void initialize(internal::PassKey);
    ErrorOr<SslContext> makeSslContext(internal::PassKey) const;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpsprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPSPROTOCOL_HPP
