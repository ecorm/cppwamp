/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tlsprotocol.hpp"
#include <cassert>
#include <boost/asio/ssl/context.hpp>
#include "../api.hpp"
#include "../exceptions.hpp"

namespace wamp
{

//******************************************************************************
// SslContext
//******************************************************************************

struct SslContext::Impl
{
    using Context = boost::asio::ssl::context;

    template <typename... Ts>
    Impl(Ts&&... args) : ctx_(std::forward<Ts>(args)...) {}

    Context ctx_;
};

CPPWAMP_INLINE SslContext::SslContext()
    : SslContext(SslVersion::tls1_2, SslVersion::unspecified)
{}

CPPWAMP_INLINE SslContext::SslContext(SslVersion min)
    : SslContext(min, SslVersion::unspecified)
{}

/** @throws error::Failure if the underlying context handle creation failed. */
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

CPPWAMP_INLINE SslContext::SslContext(boost::asio::ssl::context&& context)
    : impl_(std::make_shared<Impl>(std::move(context)))
{}

CPPWAMP_INLINE SslContext::SslContext(void* nativeHandle)
    : impl_(std::make_shared<Impl>(static_cast<SSL_CTX*>(nativeHandle)))
{}

/** Calls Asio's [ssl::context::set_options]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/set_options.html),
    which calls [SSL_CTX_set_options]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_options.html) */
CPPWAMP_INLINE ErrorOrDone SslContext::setOptions(uint64_t options)
{
    boost::system::error_code ec;
    impl_->ctx_.set_options(options, ec);
    return trueOrError(ec);
}

/** Calls Asio's [ssl::context::clear_options]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/clear_options.html),
    which calls [SSL_CTX_clear_options]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_clear_options.html) */
CPPWAMP_INLINE ErrorOrDone SslContext::clearOptions(uint64_t options)
{
    boost::system::error_code ec;
    impl_->ctx_.clear_options(options, ec);
    return trueOrError(ec);
}

/** @details
    The given certificate data must use the PEM format.

    The implementation makes a copy of the buffer data.

    Calls Asio's [ssl::context::add_certificate_authority]
    (https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/ssl__context/add_certificate_authority.html),
    which calls [SSL_CTX_get_cert_store]
    (https://www.openssl.org/docs/manmaster/man3/SSL_CTX_get_cert_store.html)
    and [X509_STORE_add_cert]
    (https://www.openssl.org/docs/manmaster/man3/X509_STORE_add_cert.html) */
CPPWAMP_INLINE ErrorOrDone SslContext::addVerifyCertificate(const void* data,
                                                            std::size_t size)
{
    boost::system::error_code ec;
    impl_->ctx_.add_certificate_authority({data, size}, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone SslContext::addVerifyPath(const std::string& path)
{
    boost::system::error_code ec;
    impl_->ctx_.add_verify_path(path, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone
SslContext::loadVerifyFile(const std::string& filename)
{
    boost::system::error_code ec;
    impl_->ctx_.load_verify_file(filename, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone SslContext::useDefaultVerifyPaths()
{
    boost::system::error_code ec;
    impl_->ctx_.set_default_verify_paths(ec);
    return trueOrError(ec);
}

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

CPPWAMP_INLINE ErrorOrDone SslContext::setVerifyDepth(int depth)
{
    boost::system::error_code ec;
    impl_->ctx_.set_verify_depth(depth, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone SslContext::setVerifyMode(int mode)
{
    boost::system::error_code ec;
    impl_->ctx_.set_verify_mode(mode, ec);
    return trueOrError(ec);
}

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

CPPWAMP_INLINE ErrorOrDone SslContext::useCertificate(
    const void* data, std::size_t size, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_certificate({data, size}, ff, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone SslContext::useCertificateFile(
    const std::string& filename, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_certificate_file(filename, ff, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone
SslContext::useCertificateChain(const void* data, std::size_t size)
{
    boost::system::error_code ec;
    impl_->ctx_.use_certificate_chain({data, size}, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone
SslContext::useCertificateChainFile(const std::string& filename)
{
    boost::system::error_code ec;
    impl_->ctx_.use_certificate_chain_file(filename, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone SslContext::usePrivateKey(
    const void* data, std::size_t size, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_private_key({data, size}, ff, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone
SslContext::usePrivateKeyFile(const std::string& filename, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_private_key_file(filename, ff, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone SslContext::useRsaPrivateKey(
    const void* data, std::size_t size, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_rsa_private_key({data, size}, ff, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone
SslContext::useRsaPrivateKeyFile(const std::string& filename, SslFileFormat format)
{
    auto ff = static_cast<boost::asio::ssl::context_base::file_format>(
        toAsioFileFormat(format));

    boost::system::error_code ec;
    impl_->ctx_.use_rsa_private_key_file(filename, ff, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone
SslContext::useTempDh(const void* data, std::size_t size)
{
    boost::system::error_code ec;
    impl_->ctx_.use_tmp_dh({data, size}, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE ErrorOrDone
SslContext::useTempDhFile(const std::string& filename)
{
    boost::system::error_code ec;
    impl_->ctx_.use_tmp_dh_file(filename, ec);
    return trueOrError(ec);
}

CPPWAMP_INLINE const boost::asio::ssl::context& SslContext::get() const
{
    return impl_->ctx_;
}

CPPWAMP_INLINE boost::asio::ssl::context& SslContext::get()
{
    return impl_->ctx_;
}

CPPWAMP_INLINE SslContext::Handle SslContext::handle()
{
    return static_cast<void*>(impl_->ctx_.native_handle());
}

CPPWAMP_INLINE ErrorOrDone SslContext::trueOrError(std::error_code ec)
{
    if (ec)
        return makeUnexpected(ec);
    return true;
}

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
// TlsHost
//******************************************************************************

CPPWAMP_INLINE TlsHost::TlsHost(std::string address, std::string serviceName,
                                SslContext context)
    : Base(std::move(address), std::move(serviceName)),
      sslContext_(std::move(context))
{}

CPPWAMP_INLINE TlsHost::TlsHost(std::string address, Port port,
                                SslContext context)
    : Base(std::move(address), std::to_string(port)),
      sslContext_(std::move(context))
{}


//******************************************************************************
// TlsEndpoint
//******************************************************************************

CPPWAMP_INLINE TlsEndpoint::TlsEndpoint(Port port, SslContext context)
    : Base("", port),
      sslContext_(std::move(context))
{
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE TlsEndpoint::TlsEndpoint(
    std::string address, unsigned short port, SslContext context)
    : Base(std::move(address), port),
      sslContext_(std::move(context))
{
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE std::string TlsEndpoint::label() const
{
    auto portString = std::to_string(port());
    if (address().empty())
        return "TLS Port " + portString;
    return "TLS " + address() + ':' + portString;
}

CPPWAMP_INLINE void TcpEndpoint::initialize(internal::PassKey) {}

} // namespace wamp
