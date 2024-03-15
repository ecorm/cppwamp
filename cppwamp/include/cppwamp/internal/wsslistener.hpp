/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WSSLISTENER_HPP
#define CPPWAMP_INTERNAL_WSSLISTENER_HPP

#include <memory>
#include "tcplistener.hpp"
#include "wsstransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct WssListenerConfig : public BasicTcpListenerConfig<WssServerTransport,
                                                         WssEndpoint>
{
    static ErrorOr<Transporting::Ptr> makeTransport(
        UnderlyingSocket&& socket, std::shared_ptr<Settings> settings,
        CodecIdSet codecIds, RouterLogger::Ptr logger)
    {
        auto sslContextOrError = settings->makeSslContext({});
        if (!sslContextOrError.has_value())
            return makeUnexpected(sslContextOrError.error());

        using Ssl = boost::beast::ssl_stream<boost::asio::ip::tcp::socket>;
        Ssl stream{std::move(socket), sslContextOrError->get()};

        auto transport = std::make_shared<Transport>(
            std::move(stream), std::move(settings), std::move(codecIds),
            std::move(logger), std::move(*sslContextOrError));

        return std::static_pointer_cast<Transporting>(transport);
    }
};


//------------------------------------------------------------------------------
class WssListener : public RawsockListener<WssListenerConfig>
{
public:
    using Ptr = std::shared_ptr<WssListener>;
    using RawsockListener<WssListenerConfig>::RawsockListener;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
