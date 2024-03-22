/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpsprotocol.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE HttpsEndpoint::HttpsEndpoint(Port port,
                                            SslContextGenerator generator)
    : HttpsEndpoint("", port, std::move(generator))
{}

CPPWAMP_INLINE HttpsEndpoint::HttpsEndpoint(std::string address, Port port,
                                            SslContextGenerator generator)
    : Base(std::move(address), port),
      sslContextGenerator_(std::move(generator))

{
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE HttpsEndpoint&
HttpsEndpoint::withOptions(HttpServerOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE HttpsEndpoint& HttpsEndpoint::addBlock(HttpServerBlock block)
{
    serverBlocks_.upsert(std::move(block));
    return *this;
}

CPPWAMP_INLINE const HttpServerOptions& HttpsEndpoint::options() const
{
    return options_;
}

CPPWAMP_INLINE HttpServerOptions& HttpsEndpoint::options() {return options_;}

CPPWAMP_INLINE HttpServerBlock* HttpsEndpoint::findBlock(std::string hostName)
{
    return serverBlocks_.find(hostName);
}

CPPWAMP_INLINE std::string HttpsEndpoint::label() const
{
    auto portString = std::to_string(port());
    if (address().empty())
        return "HTTP Port " + portString;
    return "HTTP " + address() + ':' + portString;
}

CPPWAMP_INLINE void HttpsEndpoint::initialize(internal::PassKey)
{
    options_.merge(HttpServerOptions::defaults());
    serverBlocks_.initialize(options_);
}

CPPWAMP_INLINE ErrorOr<SslContext>
HttpsEndpoint::makeSslContext(internal::PassKey) const
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
