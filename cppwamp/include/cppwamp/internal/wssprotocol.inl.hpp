/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/wssprotocol.hpp"
#include "../api.hpp"
#include "../version.hpp"
#include "httpurlvalidator.hpp"

namespace wamp
{

//******************************************************************************
// WssHost
//******************************************************************************

CPPWAMP_INLINE bool WssHost::targetIsValid(const std::string& target)
{
    return !internal::HttpUrlValidator::validateForWebsocket(target);
}

CPPWAMP_INLINE WssHost::WssHost(std::string address, std::string serviceName,
                                SslContextGenerator generator)
    : Base(std::move(address), std::move(serviceName)),
      target_("/"),
      sslContextGenerator_(std::move(generator))
{
    options_.withAgent(Version::clientAgentString());
}

CPPWAMP_INLINE WssHost::WssHost(std::string address, Port port,
                                SslContextGenerator generator)
    : WssHost(std::move(address), std::to_string(port), std::move(generator))
{}

CPPWAMP_INLINE WssHost& WssHost::withTarget(std::string target)
{
    target_ = std::move(target);
    return *this;
}

CPPWAMP_INLINE WssHost&
WssHost::withOptions(WebsocketOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE WssHost& WssHost::withSslVerifyOptions(SslVerifyOptions options)
{
    sslVerifyOptions_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE const std::string& WssHost::target() const
{
    return target_;
}

CPPWAMP_INLINE const WebsocketOptions& WssHost::options() const
{
    return options_;
}

CPPWAMP_INLINE const SslVerifyOptions& WssHost::sslVerifyOptions() const
{
    return sslVerifyOptions_;
}

CPPWAMP_INLINE ErrorOr<SslContext>
WssHost::makeSslContext(internal::PassKey) const
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
// WssEndpoint
//******************************************************************************

CPPWAMP_INLINE WssEndpoint::WssEndpoint(Port port,
                                        SslContextGenerator generator)
    : WssEndpoint("", port, std::move(generator))
{}

CPPWAMP_INLINE WssEndpoint::WssEndpoint(std::string address, Port port,
                                        SslContextGenerator generator)
    : Base(std::move(address), port),
      sslContextGenerator_(std::move(generator))
{
    options_.withAgent(Version::serverAgentString());
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE WssEndpoint&
WssEndpoint::withOptions(WebsocketOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE const WebsocketOptions& WssEndpoint::options() const
{
    return options_;
}

CPPWAMP_INLINE std::string WssEndpoint::label() const
{
    auto portString = std::to_string(port());
    if (address().empty())
        return "Websocket Port " + portString;
    return "Websocket " + address() + ':' + portString;
}

CPPWAMP_INLINE void WssEndpoint::initialize(internal::PassKey) {}

CPPWAMP_INLINE ErrorOr<SslContext>
WssEndpoint::makeSslContext(internal::PassKey) const
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
