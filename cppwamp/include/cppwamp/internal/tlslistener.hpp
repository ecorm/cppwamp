/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TLSLISTENER_HPP
#define CPPWAMP_INTERNAL_TLSLISTENER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include "../erroror.hpp"
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
    static ErrorOr<Transporting::Ptr> makeTransport(
        UnderlyingSocket&& socket, std::shared_ptr<Settings> settings,
        CodecIdSet codecIds, RouterLogger::Ptr logger)
    {
        auto sslContextOrError = settings->makeSslContext({});
        if (!sslContextOrError.has_value())
            return makeUnexpected(sslContextOrError.error());

        using Ssl = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
        Ssl stream{std::move(socket), sslContextOrError->get()};

        auto transport = std::make_shared<Transport>(
            std::move(stream), std::move(settings), std::move(codecIds),
            std::move(logger), std::move(*sslContextOrError));

        return std::static_pointer_cast<Transporting>(transport);
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
