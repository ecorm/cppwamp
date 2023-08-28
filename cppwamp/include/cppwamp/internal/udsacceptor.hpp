/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
#define CPPWAMP_INTERNAL_UDSACCEPTOR_HPP

#include <cerrno>
#include <boost/asio/local/stream_protocol.hpp>
#include "../transports/udspath.hpp"
#include "rawsockacceptor.hpp"
#include "udstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct UdsAcceptorConfig
{
    using Settings    = UdsPath;
    using NetProtocol = boost::asio::local::stream_protocol;
    using Traits      = UdsTraits;

    static NetProtocol::endpoint makeEndpoint(const Settings& s)
    {
        return {s.pathName()};
    }

    static void setAcceptorOptions(NetProtocol::acceptor&) {}

    static std::error_code onFirstEstablish(const Settings& settings)
    {
        std::error_code ec;
        if (settings.deletePathEnabled())
        {
            errno = 0;
            auto result = std::remove(settings.pathName().c_str());
            if (result != 0 && errno != 0 && errno != ENOENT)
                ec = std::error_code{errno, std::generic_category()};
        }
        return ec;
    }

    static void onDestruction(const Settings& settings)
    {
        (void)std::remove(settings.pathName().c_str());
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
        if (Helper::isAcceptCongestionError(ec))
            return ListeningErrorCategory::congestion;
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
using UdsAcceptor = RawsockAcceptor<UdsAcceptorConfig>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
