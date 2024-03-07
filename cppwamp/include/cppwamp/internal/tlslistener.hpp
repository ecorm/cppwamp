/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TLSLISTENER_HPP
#define CPPWAMP_INTERNAL_TLSLISTENER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include "rawsocklistener.hpp"
#include "rawsocktransport.hpp"
#include "tcplistener.hpp"
#include "tlstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
using TlsServerTransport = RawsockServerTransport<TlsTraits>;


//------------------------------------------------------------------------------
struct TlsListenerConfig : public BasicTcpListenerConfig<TlsServerTransport,
                                                         TlsEndpoint>
{
    static Transporting::Ptr makeTransport(
        UnderlyingSocket&& socket, std::shared_ptr<Settings> settings,
        CodecIdSet codecIds, RouterLogger::Ptr logger)
    {
        using Ssl = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
        auto sslContext = settings->makeSslContext({});
        Ssl stream{std::move(socket), sslContext.get()};
        return std::make_shared<Transport>(
            std::move(stream), std::move(settings), std::move(codecIds),
            std::move(logger), std::move(sslContext));
    }
};


//------------------------------------------------------------------------------
class TlsListener : public RawsockListener<TlsListenerConfig>
{
    using Base = RawsockListener<TlsListenerConfig>;

public:
    using Ptr = std::shared_ptr<TlsListener>;
    using Base::Base;
};

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_INTERNAL_TLSLISTENER_HPP
