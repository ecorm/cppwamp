/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TLSCONNECTOR_HPP
#define CPPWAMP_INTERNAL_TLSCONNECTOR_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include "../asiodefs.hpp"
#include "rawsockconnector.hpp"
#include "rawsocktransport.hpp"
#include "tlstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
using TlsClientTransport = RawsockClientTransport<TlsTraits>;

//------------------------------------------------------------------------------
class TlsResolver
{
public:
    using Traits    = TlsTraits;
    using Settings  = TlsHost;
    using Transport = TlsClientTransport;
    using Result    = boost::asio::ip::tcp::resolver::results_type;

    TlsResolver(IoStrand strand) : resolver_(std::move(strand)) {}

    template <typename F>
    void resolve(const Settings& settings, F&& callback)
    {
        // RawsockConnector will keep this TlsResolver object alive until
        // completion.
        resolver_.async_resolve(settings.address(), settings.serviceName(),
                                std::forward<F>(callback));
    }

    void cancel()
    {
        resolver_.cancel();
    }

private:
    boost::asio::ip::tcp::resolver resolver_;
};

//------------------------------------------------------------------------------
class TlsConnector : public RawsockConnector<TlsResolver>
{
    using Base = RawsockConnector<TlsResolver>;

public:
    using Ptr = std::shared_ptr<TlsConnector>;
    using Base::Base;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TLSCONNECTOR_HPP
