/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSLISTENER_HPP
#define CPPWAMP_INTERNAL_UDSLISTENER_HPP

#include <boost/asio/local/stream_protocol.hpp>
#include "rawsocklistener.hpp"
#include "rawsocktransport.hpp"
#include "udstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct UdsListenerConfig
{
    using Settings    = UdsEndpoint;
    using NetProtocol = boost::asio::local::stream_protocol;
    using Transport   = RawsockServerTransport<
                            BasicRawsockTransportConfig<UdsTraits>>;

    static NetProtocol::endpoint makeEndpoint(const Settings& s)
    {
        return {s.address()};
    }

    static std::error_code onFirstEstablish(const Settings& settings)
    {
        std::error_code ec;
        if (settings.deletePathEnabled())
        {
            errno = 0;
            auto result = std::remove(settings.address().c_str());
            if (result != 0 && errno != 0 && errno != ENOENT)
                ec = std::error_code{errno, std::generic_category()};
        }
        return ec;
    }

    static void onDestruction(const Settings& settings)
    {
        (void)std::remove(settings.address().c_str());
    }

    // https://stackoverflow.com/q/76955978/245265
    static ListeningErrorCategory classifyAcceptError(
        boost::system::error_code ec, bool treatUnexpectedErrorsAsFatal = false)
    {
        using Helper = SocketErrorHelper;
        if (!ec)
            return ListeningErrorCategory::success;
        if (Helper::isAcceptCancellationError(ec))
            return ListeningErrorCategory::cancelled;
        if (Helper::isAcceptOverloadError(ec))
            return ListeningErrorCategory::overload;
        if (Helper::isAcceptTransientError(ec))
            return ListeningErrorCategory::transient;
        if (treatUnexpectedErrorsAsFatal)
            return ListeningErrorCategory::fatal;
        // Treat network down errors as fatal, as there's no actual network.
        if (Helper::isAcceptFatalError(ec) || Helper::isAcceptOutageError(ec))
            return ListeningErrorCategory::fatal;
        return ListeningErrorCategory::transient;
    }
};

//------------------------------------------------------------------------------
using UdsListener = RawsockListener<UdsListenerConfig>;

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_INTERNAL_UDSLISTENER_HPP
