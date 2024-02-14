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

#include <cstdint>
#include <functional>
#include <memory>
#include "../api.hpp"
#include "../config.hpp"
#include "../erroror.hpp"
#include "tcpprotocol.hpp"

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
/** SSL/TLS protocol versions. */
//------------------------------------------------------------------------------
enum class SslVersion
{
    unspecified, ///< Don't limit minimum/maximum version
    ssl3_0,      ///< Deprecated in 2015, disabled in OpenSSL by default
    tls1_0,      ///< Deprecated in 2021
    tls1_1,      ///< Deprecated in 2021
    tls1_2,      ///< In use since 2008
    tls1_3       ///< In use since 2018
};


//------------------------------------------------------------------------------
/** Enumerates SSL/TLS password callback purposes. */
//------------------------------------------------------------------------------
enum class SslPasswordPurpose
{
    reading, ///< For reading/decryption
    writing  ///< For writing/encryption
};


//------------------------------------------------------------------------------
/** Enumerates SSL/TLS file format types. */
//------------------------------------------------------------------------------
enum class SslFileFormat
{
    asn1, ///< ASN.1 format
    pem   ///< PEM format
};


//------------------------------------------------------------------------------
/** SSL/TLS peer verification mode bits. */
//------------------------------------------------------------------------------
struct CPPWAMP_API SslVerifyMode
{
    static int none();             /**< No verification */
    static int peer();             /**< Verify the peer */
    static int failIfNoPeerCert(); /**< Fail if peer has no certificate **/
    static int clientOnce();       /**< Don't request client certificate
                                        on renegotiation. */
};


//------------------------------------------------------------------------------
/** Simple wrapper around the `X509_STORE_CTX` type, used during verification
    of a peer certificate. */
//------------------------------------------------------------------------------
class CPPWAMP_API SslVerifyContext
{
public:
    using Handle = void*;

    SslVerifyContext(Handle handle) : handle_(handle) {}

    /** Obtains the underlying native handle. */
    Handle handle() {return handle_;}

    /** Obtains the underlying native handle object pointer. */
    template <typename TObject>
    TObject* as() {return static_cast<TObject*>(handle);}

private:
    Handle handle_;
};


//------------------------------------------------------------------------------
/** Holds various configuration and data relevant to TLS session establishment.
    Wraps a reference-counted Asio `ssl::context` object, which is made
    accessible. */
//------------------------------------------------------------------------------
class CPPWAMP_API SslContext
{
public:
    using Handle = void*;

    using PasswordCallback = std::function<std::string (std::size_t maxLength,
                                                       SslPasswordPurpose)>;

    using VerifyCallback = std::function<bool (bool preverified,
                                               SslVerifyContext)>;


    /// @name Construction
    /// {
    SslContext();

    SslContext(SslVersion min);

    SslContext(SslVersion min, SslVersion max);

    SslContext(boost::asio::ssl::context&& context);

    SslContext(void* nativeHandle);
    /// }


    /// @name Options
    /// {
    // SSL_CTX_set_options
    CPPWAMP_NODISCARD ErrorOrDone setOptions(uint64_t options);

    // SSL_CTX_clear_options
    CPPWAMP_NODISCARD ErrorOrDone clearOptions(uint64_t options);
    /// }


    /// @name Verification
    /// {
    /** Adds a certification authority certificate, from a memory buffer, for
        performing verification. */
    CPPWAMP_NODISCARD ErrorOrDone addVerifyCertificate(const void* data,
                                                       std::size_t size);

    CPPWAMP_NODISCARD ErrorOrDone addVerifyPath(const std::string& path);

    CPPWAMP_NODISCARD ErrorOrDone loadVerifyFile(const std::string& filename);

    CPPWAMP_NODISCARD ErrorOrDone useDefaultVerifyPaths();

    CPPWAMP_NODISCARD ErrorOrDone setVerifyCallback(VerifyCallback cb);

    CPPWAMP_NODISCARD ErrorOrDone setVerifyDepth(int depth);

    CPPWAMP_NODISCARD ErrorOrDone setVerifyMode(int mode);
    /// }

    CPPWAMP_NODISCARD ErrorOrDone setPasswordCallback(PasswordCallback cb);

    CPPWAMP_NODISCARD ErrorOrDone
    useCertificate(const void* data, std::size_t size, SslFileFormat format);

    CPPWAMP_NODISCARD ErrorOrDone
    useCertificateFile(const std::string& filename, SslFileFormat format);

    CPPWAMP_NODISCARD ErrorOrDone
    useCertificateChain(const void* data, std::size_t size);

    CPPWAMP_NODISCARD ErrorOrDone
    useCertificateChainFile(const std::string& filename);

    CPPWAMP_NODISCARD ErrorOrDone
    usePrivateKey(const void* data, std::size_t size, SslFileFormat format);

    CPPWAMP_NODISCARD ErrorOrDone
    usePrivateKeyFile(const std::string& filename, SslFileFormat format);

    CPPWAMP_NODISCARD ErrorOrDone
    useRsaPrivateKey(const void* data, std::size_t size, SslFileFormat format);

    CPPWAMP_NODISCARD ErrorOrDone
    useRsaPrivateKeyFile(const std::string& filename, SslFileFormat format);

    CPPWAMP_NODISCARD ErrorOrDone
    useTempDh(const void* data, std::size_t size);

    CPPWAMP_NODISCARD ErrorOrDone
    useTempDhFile(const std::string& filename);

    /** Accesses the underlying Asio ssl::context object. */
    const boost::asio::ssl::context& get() const;

    /** Accesses the underlying Asio ssl::context object. */
    boost::asio::ssl::context& get();

    /** Obtains the underlying native handle. */
    Handle handle();

    /** Obtains the underlying native handle object pointer. */
    template <typename TObject>
    TObject* as();

private:
    struct Impl;
    struct ConstructorTag {};

    static ErrorOrDone trueOrError(std::error_code ec);

    static int toNativeVersion(SslVersion v);

    static int toAsioFileFormat(SslFileFormat f);

    std::error_code translateNativeError(long error);

    std::shared_ptr<Impl> impl_;
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
    TlsHost(std::string address, std::string serviceName, SslContext context);

    /** Constructor taking an URL/IP and a numeric port number. */
    TlsHost(std::string address, Port port, SslContext context);

private:
    using Base = SocketHost<TlsHost, Tls, TcpOptions, RawsockClientLimits>;

    SslContext sslContext_;
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
    explicit TlsEndpoint(Port port, SslContext context);

    /** Constructor taking an address string and a port number. */
    TlsEndpoint(std::string address, unsigned short port, SslContext context);

    /** Generates a human-friendly string of the TLS address/port. */
    std::string label() const;

private:
    using Base = SocketEndpoint<TlsEndpoint, Tls, TcpOptions,
                                RawsockServerLimits>;

    SslContext sslContext_;

public: // Internal use only
    void initialize(internal::PassKey);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tlsprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TLSPROTOCOL_HPP
