/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPCONNECTOR_HPP
#define CPPWAMP_INTERNAL_TCPCONNECTOR_HPP

#include <boost/asio/ip/tcp.hpp>
#include "../asiodefs.hpp"
#include "rawsockconnector.hpp"
#include "rawsocktransport.hpp"
#include "tcptraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class TcpResolver
{
public:
    using Settings = TcpHost;
    using Socket   = TcpTraits::NetProtocol::socket;
    using Result   = boost::asio::ip::tcp::resolver::results_type;

    TcpResolver(IoStrand strand) : resolver_(std::move(strand)) {}

    template <typename F>
    void resolve(const Settings& settings, F&& callback)
    {
        // RawsockConnector will keep this TcpResolver object alive until
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
template <typename TTransport>
using BasicTcpConnectorConfig =
    BasicRawsockConnectorConfig<TcpTraits, TcpResolver, TTransport>;

//------------------------------------------------------------------------------
using TcpConnectorConfig =
    BasicTcpConnectorConfig<
        RawsockClientTransport<BasicRawsockTransportConfig<TcpTraits>>>;

//------------------------------------------------------------------------------
class TcpConnector : public RawsockConnector<TcpConnectorConfig>
{
public:
    using Ptr = std::shared_ptr<TcpConnector>;
    using RawsockConnector<TcpConnectorConfig>::RawsockConnector;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TCPCONNECTOR_HPP
