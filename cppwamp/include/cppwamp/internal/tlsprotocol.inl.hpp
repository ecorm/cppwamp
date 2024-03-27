/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tlsprotocol.hpp"
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// TlsHost
//******************************************************************************

CPPWAMP_INLINE TlsHost::TlsHost(std::string address, std::string serviceName,
                                SslContextGenerator generator)
    : Base(std::move(address), std::move(serviceName)),
      sslContextGenerator_(std::move(generator))
{}

CPPWAMP_INLINE TlsHost::TlsHost(std::string address, Port port,
                                SslContextGenerator generator)
    : Base(std::move(address), std::to_string(port)),
      sslContextGenerator_(std::move(generator))
{}

CPPWAMP_INLINE TlsHost& TlsHost::withSslVerifyOptions(SslVerifyOptions options)
{
    sslVerifyOptions_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE const SslVerifyOptions& TlsHost::sslVerifyOptions() const
{
    return sslVerifyOptions_;
}

CPPWAMP_INLINE ErrorOr<SslContext>
TlsHost::makeSslContext(internal::PassKey) const
{
    ErrorOr<SslContext> context;

    try
    {
        context = sslContextGenerator_();
    }
    catch (const boost::system::system_error& e)
    {
        return makeUnexpected(static_cast<std::error_code>(e.code()));
    }
    catch (const std::system_error& e)
    {
        return makeUnexpected(e.code());
    }

    return context;
}


//******************************************************************************
// TlsEndpoint
//******************************************************************************

CPPWAMP_INLINE TlsEndpoint::TlsEndpoint(Port port,
                                        SslContextGenerator generator)
    : Base("", port),
      sslContextGenerator_(std::move(generator))
{
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE TlsEndpoint::TlsEndpoint(
    std::string address, unsigned short port, SslContextGenerator generator)
    : Base(std::move(address), port),
      sslContextGenerator_(std::move(generator))
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

CPPWAMP_INLINE void TlsEndpoint::initialize(internal::PassKey) {}

CPPWAMP_INLINE ErrorOr<SslContext>
TlsEndpoint::makeSslContext(internal::PassKey) const
{
    ErrorOr<SslContext> context;

    try
    {
        context = sslContextGenerator_();
    }
    catch (const boost::system::system_error& e)
    {
        return makeUnexpected(static_cast<std::error_code>(e.code()));
    }
    catch (const std::system_error& e)
    {
        return makeUnexpected(e.code());
    }

    return context;
}

} // namespace wamp