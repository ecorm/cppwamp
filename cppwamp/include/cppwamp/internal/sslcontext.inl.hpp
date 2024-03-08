/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/sslcontext.hpp"
#include <cassert>
#include <boost/asio/ssl/context.hpp>
#include "../api.hpp"
#include "../exceptions.hpp"

namespace wamp
{

//******************************************************************************
// SslVerifyMode
//******************************************************************************

CPPWAMP_INLINE int SslVerifyMode::none()
{
    return boost::asio::ssl::verify_none;
}

CPPWAMP_INLINE int SslVerifyMode::peer()
{
    return boost::asio::ssl::verify_peer;
}

/** @note Ignored unless ssl::verify_peer is set. */
CPPWAMP_INLINE int SslVerifyMode::failIfNoPeerCert()
{
    return boost::asio::ssl::verify_fail_if_no_peer_cert;
}

/** @note Ignored unless ssl::verify_peer is set. */
CPPWAMP_INLINE int SslVerifyMode::clientOnce()
{
    return boost::asio::ssl::verify_client_once;
}


//******************************************************************************
// SslContext
//******************************************************************************

//------------------------------------------------------------------------------
struct SslContext::Impl
{
    using Context = boost::asio::ssl::context;

    template <typename... Ts>
    Impl(Ts&&... args) : ctx_(std::forward<Ts>(args)...) {}

    Context ctx_;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE SslContext::SslContext()
    : SslContext(SslVersion::tls1_2, SslVersion::unspecified)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SslContext::SslContext(SslVersion min)
    : SslContext(min, SslVersion::unspecified)
{}

//------------------------------------------------------------------------------
/** @throws error::Failure if the underlying context handle creation failed. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE SslContext::SslContext(SslVersion min, SslVersion max)
{
    ::ERR_clear_error();

    auto handle = ::SSL_CTX_new(::TLS_method());
    if (handle == 0)
    {
        auto ec = translateNativeError(::ERR_get_error());
        throw error::Failure(
            ec, "wamp::SslContext::SslContext: SSL_CTX_new failed");
    }

    if (min != SslVersion::unspecified)
        SSL_CTX_set_min_proto_version(handle, toNativeVersion(min));

    if (max != SslVersion::unspecified)
        SSL_CTX_set_max_proto_version(handle, toNativeVersion(max));

    impl_ = std::make_shared<Impl>(handle);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SslContext::SslContext(boost::asio::ssl::context&& context)
    : impl_(std::make_shared<Impl>(std::move(context)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SslContext::SslContext(void* nativeHandle)
    : impl_(std::make_shared<Impl>(static_cast<SSL_CTX*>(nativeHandle)))
{}

//------------------------------------------------------------------------------
/** Calls Asio's [ssl::context::set_options]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/set_options.html),
    which calls [SSL_CTX_set_options]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_options.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::setOptions(uint64_t options)
{
    boost::system::error_code ec;
    impl_->ctx_.set_options(options, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** Calls Asio's [ssl::context::clear_options]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/clear_options.html),
    which calls [SSL_CTX_clear_options]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_clear_options.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::clearOptions(uint64_t options)
{
    boost::system::error_code ec;
    impl_->ctx_.clear_options(options, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    The given certificate data must use the PEM format.

    Calls Asio's [ssl::context::add_certificate_authority]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/add_certificate_authority.html),
    which calls [SSL_CTX_get_cert_store]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_get_cert_store.html)
    and [X509_STORE_add_cert]
    (https://www.openssl.org/docs/manmaster/man3/X509_STORE_add_cert.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::addVerifyCertificate(const void* data,
                                                            std::size_t size)
{
    boost::system::error_code ec;
    impl_->ctx_.add_certificate_authority({data, size}, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Each file in the directory must contain a single certificate. The files
    must be named using the subject name's hash and an extension of ".0".

    Calls Asio's [ssl::context::add_verify_path]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/add_verify_path.html)
    which calls [SSL_CTX_load_verify_locations]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_load_verify_locations.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::addVerifyPath(const std::string& path)
{
    boost::system::error_code ec;
    impl_->ctx_.add_verify_path(path, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    The given filename is for a file containing certification authority
    certificates in PEM format.

    Calls Asio's [ssl::context::load_verify_file]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/add_verify_path.html)
    which calls [SSL_CTX_load_verify_locations]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_load_verify_locations.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone
SslContext::loadVerifyFile(const std::string& filename)
{
    boost::system::error_code ec;
    impl_->ctx_.load_verify_file(filename, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    From the OpenSSL SSL_CTX_set_default_verify_paths man page:

    > There is one default directory, one default file and one default store.
    > The default CA certificates directory is called `certs` in the default
    > OpenSSL directory, and this is also the default store. Alternatively the
    > `SSL_CERT_DIR` environment variable can be defined to override this
    > location. The default CA certificates file is called `cert.pem` in the
    > default OpenSSL directory. Alternatively the `SSL_CERT_FILE` environment
    > variable can be defined to override this location.

    Calls Asio's [ssl::context::set_default_verify_paths]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/set_default_verify_paths.html)
    which calls [SSL_CTX_set_default_verify_paths]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_default_verify_paths.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::resetVerifyPathsToDefault()
{
    boost::system::error_code ec;
    impl_->ctx_.set_default_verify_paths(ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    From the Asio SSL client example:

    > The verify callback can be used to check whether the certificate that is
    > being presented is valid for the peer. For example, RFC 2818 describes
    > the steps involved in doing this for HTTPS. Consult the OpenSSL
    > documentation for more details. Note that the callback is called once
    > for each certificate in the certificate chain, starting from the root
    > certificate authority.

    The function signature of the handler must be:
    ```
    bool callback(
        bool preverified, // True if the certicate passed pre-verification
        SslVerifyContext  // Context containing the peer certificate
    );
    ```

    Calls Asio's [ssl::context::set_verify_callback]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/set_verify_callback.html)
    which calls [SSL_CTX_set_verify]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_verify.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::setVerifyCallback(VerifyCallback cb)
{
    struct Callback
    {
        using AsioVerifyContext = boost::asio::ssl::verify_context;

        VerifyCallback callback;

        bool operator()(bool preverified, AsioVerifyContext& ctx)
        {
            return callback(preverified, SslVerifyContext{ctx.native_handle()});
        }
    };

    boost::system::error_code ec;
    impl_->ctx_.set_verify_callback(Callback{std::move(cb)}, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::set_verify_depth]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/set_verify_depth.html)
    which calls [SSL_CTX_set_verify_depth]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_verify_depth.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::setVerifyDepth(int depth)
{
    boost::system::error_code ec;
    impl_->ctx_.set_verify_depth(depth, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::set_verify_mode]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/set_verify_mode.html)
    which calls [SSL_CTX_set_verify]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_verify.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::setVerifyMode(
    int mode ///< Bitwise-ORed wamp::SslVerifyMode flags
    )
{
    boost::system::error_code ec;
    impl_->ctx_.set_verify_mode(mode, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    The callback function signature must be:
    ```
    std::string callback(
        std::size_t maxLength,   // The maximum size for a password.
        wamp::SslPasswordPurpose // Whether password is for reading or writing.
    );
    ```

    The callback shall return a string containing the password.

    Calls Asio's [ssl::context::set_password_callback]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/set_password_callback.html)
    which calls [SSL_CTX_set_default_passwd_cb]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_verify.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::setPasswordCallback(PasswordCallback cb)
{
    struct Callback
    {
        using ContextBase = boost::asio::ssl::context_base;

        PasswordCallback callback;

        std::string operator()(std::size_t maxLength,
                               ContextBase::password_purpose p)
        {
            SslPasswordPurpose purpose;

            switch (p)
            {
            case ContextBase::for_reading:
                purpose = SslPasswordPurpose::reading;
                break;

            case ContextBase::for_writing:
                purpose = SslPasswordPurpose::writing;
                break;

            default:
                assert(false &&
                       "Unexpected "
                       "boost::asio::ssl::context_base::password_purpose "
                       "enumerator");
                break;
            }

            return callback(maxLength, purpose);
        }
    };

    boost::system::error_code ec;
    impl_->ctx_.set_password_callback(Callback{std::move(cb)}, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::use_certificate]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_certificate.html)
    which calls [SSL_CTX_use_certificate]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_certificate.html)
    or [SSL_CTX_use_certificate_ASN1]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_certificate_ASN1.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::useCertificate(
    const void* data, std::size_t size, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_certificate({data, size}, ff, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::use_certificate_file]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_certificate_file.html)
    which calls [SSL_CTX_use_certificate_file]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_certificate_file.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::useCertificateFile(
    const std::string& filename, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_certificate_file(filename, ff, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    The certificate chain must use the PEM format.

    Calls Asio's [ssl::context::use_certificate_chain]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_certificate_chain.html)
    which calls [SSL_CTX_use_certificate]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_certificate.html)
    and [SSL_CTX_add_extra_chain_cert]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_add_extra_chain_cert.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone
SslContext::useCertificateChain(const void* data, std::size_t size)
{
    boost::system::error_code ec;
    impl_->ctx_.use_certificate_chain({data, size}, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    The file must use the PEM format.

    Calls Asio's [ssl::context::use_certificate_chain_file]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_certificate_chain_file.html)
    which calls [SSL_CTX_use_certificate_chain_file]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_certificate_chain_file.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone
SslContext::useCertificateChainFile(const std::string& filename)
{
    boost::system::error_code ec;
    impl_->ctx_.use_certificate_chain_file(filename, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::use_private_key]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_private_key.html)
    which calls [SSL_CTX_use_PrivateKey]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_certificate_chain_file.html)
    or [SSL_CTX_use_PrivateKey_ASN1]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_PrivateKey_ASN1.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::usePrivateKey(
    const void* data, std::size_t size, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_private_key({data, size}, ff, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::use_private_key_file]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_private_key_file.html)
    which calls [SSL_CTX_use_PrivateKey_file]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_PrivateKey_file.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone
SslContext::usePrivateKeyFile(const std::string& filename, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_private_key_file(filename, ff, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::use_rsa_private_key]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_rsa_private_key.html)
    which calls [SSL_CTX_use_RSAPrivateKey]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_RSAPrivateKey.html)
    or [SSL_CTX_use_RSAPrivateKey_ASN1]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_RSAPrivateKey_ASN1.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::useRsaPrivateKey(
    const void* data, std::size_t size, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_rsa_private_key({data, size}, ff, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    Calls Asio's [ssl::context::use_rsa_private_key_file]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_rsa_private_key_file.html)
    which calls [SSL_CTX_use_RSAPrivateKey_file]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_use_RSAPrivateKey_file.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone
SslContext::useRsaPrivateKeyFile(const std::string& filename, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_rsa_private_key_file(filename, ff, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    The buffer must use the PEM format.

    Calls Asio's [ssl::context::use_tmp_dh]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_rsa_private_key_file.html)
    which calls [SSL_CTX_set_tmp_dh]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_tmp_dh.html).

    @note The underlying `SSL_CTX_set_tmp_dh` function is deprecated in
            OpenSSL >= 3.0, in favor of SSL_CTX_set_dh_auto.
            Use SslContext::enableAutoDh instead if available. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone
SslContext::useTempDh(const void* data, std::size_t size)
{
    boost::system::error_code ec;
    impl_->ctx_.use_tmp_dh({data, size}, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** @details
    The file must use the PEM format.

    Calls Asio's [ssl::context::use_tmp_dh_file]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/use_tmp_dh_file.html)
    which calls [SSL_CTX_set_tmp_dh]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_tmp_dh.html).

    @note The underlying `SSL_CTX_set_tmp_dh` function is deprecated in
          OpenSSL >= 3.0, in favor of SSL_CTX_set_dh_auto.
          Use SslContext::enableAutoDh instead if available. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone
SslContext::useTempDhFile(const std::string& filename)
{
    boost::system::error_code ec;
    impl_->ctx_.use_tmp_dh_file(filename, ec);
    return trueOrError(ec);
}

//------------------------------------------------------------------------------
/** Returns true if the `OPENSSL_VERSION_NUMBER` macro is greater than or
    equal to 3.0.0. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE bool SslContext::hasAutoDh() const
{
#ifdef CPPWAMP_SSL_AUTO_DIFFIE_HELLMAN_AVAILABLE
    return true;
#else
    return false;
#endif
}

//------------------------------------------------------------------------------
/** If available, calls [SSL_CTX_set_dh_auto]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_dh_auto.html).
    Otherwise, returns wamp::MiscErrc::absent. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::enableAutoDh(bool enabled)
{
#ifdef CPPWAMP_SSL_AUTO_DIFFIE_HELLMAN_AVAILABLE
    return makeUnexpectedError(MiscErrc::absent);
#else
    ::ERR_clear_error();
    int onOff = enabled ? 1 : 0;
    auto ok = ::SSL_CTX_set_dh_auto(impl_->ctx_.native_handle(), onOff);
    if (ok != 0)
        return true;
    auto ec = translateNativeError(::ERR_get_error());
    return makeUnexpected(ec);
#endif
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const boost::asio::ssl::context& SslContext::get() const
{
    return impl_->ctx_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE boost::asio::ssl::context& SslContext::get()
{
    return impl_->ctx_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE SslContext::Handle SslContext::handle()
{
    return static_cast<void*>(impl_->ctx_.native_handle());
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ErrorOrDone SslContext::trueOrError(std::error_code ec)
{
    if (ec)
        return makeUnexpected(ec);
    return true;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE int SslContext::toNativeVersion(SslVersion v)
{
    switch (v)
    {
    case SslVersion::ssl3_0: return SSL3_VERSION;
    case SslVersion::tls1_0: return TLS1_VERSION;
    case SslVersion::tls1_1: return TLS1_1_VERSION;
    case SslVersion::tls1_2: return TLS1_2_VERSION;
    case SslVersion::tls1_3: return TLS1_3_VERSION;
    default:                 break;
    }

    assert(false && "Unexpected SslVersion enumerator");
    return 0;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE int SslContext::toAsioFileFormat(SslFileFormat f)
{
    using AsioContextBase = boost::asio::ssl::context_base;

    switch (f)
    {
    case SslFileFormat::asn1: return AsioContextBase::asn1;
    case SslFileFormat::pem:  return AsioContextBase::pem;
    default:                  break;
    }

    assert(false &&
           "Unexpected boost::asio::ssl::context_base::file_format enumerator");
    return 0;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::error_code SslContext::translateNativeError(long error)
{
    // Borrowed from boost::asio::ssl::context::translate_error
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
    if (ERR_SYSTEM_ERROR(error))
    {
        return boost::system::error_code(
            static_cast<int>(ERR_GET_REASON(error)),
            boost::asio::error::get_system_category());
    }
#endif // (OPENSSL_VERSION_NUMBER >= 0x30000000L)

    return boost::system::error_code(static_cast<int>(error),
                                     boost::asio::error::get_ssl_category());
}


//******************************************************************************
// SslVerifyOptions
//******************************************************************************

//------------------------------------------------------------------------------
/** @details
    From the Asio SSL client example:

    > The verify callback can be used to check whether the certificate that is
    > being presented is valid for the peer. For example, RFC 2818 describes
    > the steps involved in doing this for HTTPS. Consult the OpenSSL
    > documentation for more details. Note that the callback is called once
    > for each certificate in the certificate chain, starting from the root
    > certificate authority.

    The function signature of the handler must be:
    ```
    bool callback(
        bool preverified, // True if the certicate passed pre-verification
        SslVerifyContext  // Context containing the peer certificate
    );
    ```

    Upon construction of the underlying socket, calls Asio's
    [ssl::stream::set_verify_callback]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__stream/set_verify_callback.html)
    which calls [SSL_set_verify]
    (https://www.openssl.org/docs/manmaster/man3/SSL_set_verify.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE SslVerifyOptions&
SslVerifyOptions::withCallback(VerifyCallback callback)
{
    callback_ = std::move(callback);
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    Upon construction of the underlying socket, calls Asio's
    [ssl::stream::set_verify_depth]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__stream/set_verify_depth.html)
    which calls [SSL_set_verify_depth]
    (https://www.openssl.org/docs/manmaster/man3/SSL_set_verify_depth.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE SslVerifyOptions& SslVerifyOptions::withDepth(int depth)
{
    depth_ = depth;
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    Upon construction of the underlying socket, calls Asio's
    [ssl::stream::set_verify_mode]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__stream/set_verify_mode.html)
    which calls [SSL_set_verify]
    (https://www.openssl.org/docs/manmaster/man3/SSL_set_verify.html). */
//------------------------------------------------------------------------------
CPPWAMP_INLINE SslVerifyOptions& SslVerifyOptions::withMode(
    int mode ///< Bitwise-ORed wamp::SslVerifyMode flags
    )
{
    mode_ = mode;
    modeIsSpecified_ = true;
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const SslVerifyOptions::VerifyCallback&
SslVerifyOptions::callback() const
{
    return callback_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE int SslVerifyOptions::depth() const {return depth_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE int SslVerifyOptions::mode() const {return mode_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool SslVerifyOptions::modeIsSpecified() const
{
    return modeIsSpecified_;
}

} // namespace wamp
