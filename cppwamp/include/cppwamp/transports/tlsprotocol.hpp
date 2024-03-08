/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TLSPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_TLSPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic TLS protocol facilities. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "../erroror.hpp"
#include "sslcontext.hpp"
#include "tcpprotocol.hpp"
#include "../internal/passkey.hpp"

// Determines whether OpenSSL's `SSL_CTX_set_dh_auto` function is available.
#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x30000000L
    #define CPPWAMP_SSL_AUTO_DIFFIE_HELLMAN_AVAILABLE 1
#endif

// Forward declarations
namespace boost { namespace asio { namespace ssl { class context; }}}


namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the TLS transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Tls
{
    constexpr Tls() = default;
};


//------------------------------------------------------------------------------
/** Contains TLS host address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API TlsHost
    : public SocketHost<TlsHost, Tls, TcpOptions, RawsockClientLimits>
{
public:
    /** Constructor taking an URL/IP and a service string. */
    TlsHost(std::string address, std::string serviceName,
            SslContextGenerator generator);

    /** Constructor taking an URL/IP and a numeric port number. */
    TlsHost(std::string address, Port port, SslContextGenerator generator);

    /** Specifies the SSL peer verification options. */
    TlsHost& withSslVerifyOptions(SslVerifyOptions options);

    /** Obtains the SSL peer verification options. */
    const SslVerifyOptions& sslVerifyOptions() const;

private:
    using Base = SocketHost<TlsHost, Tls, TcpOptions, RawsockClientLimits>;

    SslContextGenerator sslContextGenerator_;
    SslVerifyOptions sslVerifyOptions_;

public: // Internal use only
    ErrorOr<SslContext> makeSslContext(internal::PassKey) const;
};


//------------------------------------------------------------------------------
/** Contains TLS server address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API TlsEndpoint
    : public SocketEndpoint<TlsEndpoint, Tls, TcpOptions, RawsockServerLimits>
{
public:
    /** Constructor taking a port number. */
    explicit TlsEndpoint(Port port, SslContextGenerator generator);

    /** Constructor taking an address string and a port number. */
    TlsEndpoint(std::string address, unsigned short port,
                SslContextGenerator generator);

    /** Generates a human-friendly string of the TLS address/port. */
    std::string label() const;

private:
    using Base = SocketEndpoint<TlsEndpoint, Tls, TcpOptions,
                                RawsockServerLimits>;

    SslContextGenerator sslContextGenerator_;

public: // Internal use only
    void initialize(internal::PassKey);
    ErrorOr<SslContext> makeSslContext(internal::PassKey) const;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tlsprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TLSPROTOCOL_HPP
