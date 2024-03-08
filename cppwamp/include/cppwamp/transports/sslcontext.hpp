/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_SSLCONTEXT_HPP
#define CPPWAMP_TRANSPORTS_SSLCONTEXT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying SSL/TLS options. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include "../api.hpp"
#include "../config.hpp"
#include "../erroror.hpp"

// Determines whether OpenSSL's `SSL_CTX_set_dh_auto` function is available.
#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x30000000L
    #define CPPWAMP_SSL_AUTO_DIFFIE_HELLMAN_AVAILABLE 1
#endif

// Forward declarations
namespace boost { namespace asio { namespace ssl { class context; }}}


namespace wamp
{

//------------------------------------------------------------------------------
/** SSL/TLS protocol versions. */
//------------------------------------------------------------------------------
enum class SslVersion
{
    unspecified, ///< Don't limit minimum or maximum version
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
    /// Opaque native handle type.
    using Handle = void*;

    /** Constructor taking an opaque native handle. */
    explicit SslVerifyContext(Handle handle) : handle_(handle) {}

    /** Obtains an opaque pointer to the native `X509_STORE_CTX` type. */
    Handle handle() {return handle_;}

    /** Obtains the underlying native object pointer.
        @tparam TObject Should be X509_STORE_CTX */
    template <typename TObject>
    TObject* as() {return static_cast<TObject*>(handle_);}

private:
    Handle handle_;
};


//------------------------------------------------------------------------------
/** Holds various configuration and data relevant to TLS session establishment.
    Wraps a reference-counted Asio `ssl::context` object, which can be
    directly accessed. */
//------------------------------------------------------------------------------
class CPPWAMP_API SslContext
{
public:
    /// Opaque native handle type.
    using Handle = void*;

    /// Callback function type used for verifying peer certificates.
    using VerifyCallback = std::function<bool (bool preverified,
                                              SslVerifyContext)>;

    /// Callback function type used obtain password information.
    using PasswordCallback = std::function<std::string (std::size_t maxLength,
                                                       SslPasswordPurpose)>;

    /// @name Construction
    /// {
    /** Default constructor using TLS1.2 as the minimum version. */
    SslContext();

    /** Constructor taking a minium SSL/TLS version. */
    explicit SslContext(SslVersion min);

    /** Constructor taking minium and maximum SSL/TLS versions. */
    SslContext(SslVersion min, SslVersion max);

    /** Constructor taking ownership of the given Asio `ssl::context` object. */
    SslContext(boost::asio::ssl::context&& context);

    /** Constructor taking ownership of the given native handle. */
    SslContext(void* nativeHandle);
    /// }


    /// @name Options
    /// {
    /** Sets options using the given Asio `ssl::context_base` flags. */
    CPPWAMP_NODISCARD ErrorOrDone setOptions(uint64_t options);

    /** Clears options of the underlying context object. */
    CPPWAMP_NODISCARD ErrorOrDone clearOptions(uint64_t options);
    /// }


    /// @name Verification
    /// {
    /** Loads a certification authority certificate, from a memory buffer, for
        performing verification. */
    CPPWAMP_NODISCARD ErrorOrDone addVerifyCertificate(const void* data,
                                                       std::size_t size);
    /** Adds a directory containing certificate authority files to be used for
        performing verification. */
    CPPWAMP_NODISCARD ErrorOrDone addVerifyPath(const std::string& path);

    /** Loads a certification authority file for performing verification. */
    CPPWAMP_NODISCARD ErrorOrDone loadVerifyFile(const std::string& filename);

    /** Configures the context to use the default directories for finding
        certification authority certificates. */
    CPPWAMP_NODISCARD ErrorOrDone resetVerifyPathsToDefault();

    /** Sets the callback used to verify peer certificates. */
    CPPWAMP_NODISCARD ErrorOrDone setVerifyCallback(VerifyCallback cb);

    /** Sets the maximum depth for the certificate chain verification. */
    CPPWAMP_NODISCARD ErrorOrDone setVerifyDepth(int depth);

    /** Set the peer verification mode. */
    CPPWAMP_NODISCARD ErrorOrDone setVerifyMode(int mode);
    /// }

    /// @name Server Certificates
    /// {
    /** Loads a certificate from a memory buffer. */
    CPPWAMP_NODISCARD ErrorOrDone
    useCertificate(const void* data, std::size_t size, SslFileFormat format);

    /** Loads a certificate from a file. */
    CPPWAMP_NODISCARD ErrorOrDone
    useCertificateFile(const std::string& filename, SslFileFormat format);

    /** Loads a certificate chain from a memory buffer. */
    CPPWAMP_NODISCARD ErrorOrDone
    useCertificateChain(const void* data, std::size_t size);

    /** Loads a certificate chain from a file. */
    CPPWAMP_NODISCARD ErrorOrDone
    useCertificateChainFile(const std::string& filename);
    /// }

    /// @name Private Keys
    /// {
    /** Specifies a callback function for obtaining password information
        about a PEM-formatted encrypted key. */
    CPPWAMP_NODISCARD ErrorOrDone setPasswordCallback(PasswordCallback cb);

    /** Loads a private key from a memory buffer. */
    CPPWAMP_NODISCARD ErrorOrDone
    usePrivateKey(const void* data, std::size_t size, SslFileFormat format);

    /** Loads a private key from a file. */
    CPPWAMP_NODISCARD ErrorOrDone
    usePrivateKeyFile(const std::string& filename, SslFileFormat format);

    /** Loads an RSA private key from a memory buffer. */
    CPPWAMP_NODISCARD ErrorOrDone
    useRsaPrivateKey(const void* data, std::size_t size, SslFileFormat format);

    /** Loads an RSA private key from a file. */
    CPPWAMP_NODISCARD ErrorOrDone
    useRsaPrivateKeyFile(const std::string& filename, SslFileFormat format);
    /// }

    /// @name Diffie-Hellman KeyExchange
    /// {
    /** Load temporary Diffie-Hellman parameters from a memory buffer. */
    CPPWAMP_NODISCARD ErrorOrDone
    useTempDh(const void* data, std::size_t size);

    /** Load temporary Diffie-Hellman parameters from a file. */
    CPPWAMP_NODISCARD ErrorOrDone
    useTempDhFile(const std::string& filename);

    /** Indicates if the automatic built-in Diffie-Hellman parameters
        are available. */
    bool hasAutoDh() const;

    /** Use automatic built-in Diffie-Hellman parameters. */
    CPPWAMP_NODISCARD ErrorOrDone enableAutoDh(bool enabled = true);
    /// }

    /// @name SSL Context Access
    /// {
    /** Accesses the underlying Asio ssl::context object. */
    const boost::asio::ssl::context& get() const;

    /** Accesses the underlying Asio ssl::context object. */
    boost::asio::ssl::context& get();

    /** Obtains an opaque pointer to the underlying SSL_CTX object. */
    Handle handle();

    /** Obtains the underlying native handle object pointer.
        @tparam TObject Should be SSL_CTX */
    template <typename TObject>
    TObject* as() {return static_cast<TObject*>(handle());}
    /// }

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
/** Function type used to generate SslContext objects on demand. */
//------------------------------------------------------------------------------
using SslContextGenerator = std::function<ErrorOr<SslContext> ()>;


//------------------------------------------------------------------------------
/** Contains client options for verifying SSL peers. */
//------------------------------------------------------------------------------
class CPPWAMP_API SslVerifyOptions
{
public:
    /// Function type used for SSL verify callbacks.
    using VerifyCallback = std::function<bool (bool preverified,
                                              SslVerifyContext)>;

    /** Sets the callback used to verify SSL peer certificates. */
    SslVerifyOptions& withCallback(VerifyCallback callback);

    /** Sets the maximum depth for the SSL certificate chain verification. */
    SslVerifyOptions& withDepth(int depth);

    /** Set the SSL peer verification mode. */
    SslVerifyOptions& withMode(int mode);

    /** Obtains the callback used to verify SSL peer certificates. */
    const VerifyCallback& callback() const;

    /** Sets the maximum depth for the SSL certificate chain verification. */
    int depth() const;

    /** Obtains the SSL peer verification mode. */
    int mode() const;

    /** Indicates whether the SSL peer verification mode was specified. */
    bool modeIsSpecified() const;

private:
    VerifyCallback callback_;
    int depth_ = 0;
    int mode_ = 0;
    bool modeIsSpecified_ = false;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/sslcontext.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_SSLCONTEXT_HPP
