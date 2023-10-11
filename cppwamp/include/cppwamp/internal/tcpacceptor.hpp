/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPACCEPTOR_HPP
#define CPPWAMP_INTERNAL_TCPACCEPTOR_HPP

#include <boost/asio/ip/tcp.hpp>
#include "../transports/tcpprotocol.hpp"
#include "rawsockacceptor.hpp"
#include "tcptraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TSettings>
struct BasicTcpAcceptorConfig
{
    using Settings    = TSettings;
    using NetProtocol = boost::asio::ip::tcp;
    using Traits      = TcpTraits;

    static NetProtocol::endpoint makeEndpoint(const Settings& s)
    {
        namespace ip = boost::asio::ip;
        if (s.address().empty())
            return {ip::tcp::v4(), s.port()};
        return {ip::make_address(s.address()), s.port()};
    }

    static std::error_code onFirstEstablish(const Settings&) {return {};}

    static void onDestruction(const Settings&) {}

    // https://stackoverflow.com/q/76955978/245265
    static ListenStatus classifyAcceptError(
        boost::system::error_code ec, bool treatUnexpectedErrorsAsFatal)
    {
        using Helper = SocketErrorHelper;
        if (!ec)
            return ListenStatus::success;
        if (Helper::isAcceptCancellationError(ec))
            return ListenStatus::cancelled;
        if (Helper::isAcceptOverloadError(ec))
            return ListenStatus::overload;
        if (Helper::isAcceptOutageError(ec))
            return ListenStatus::outage;
        if (Helper::isAcceptTransientError(ec))
            return ListenStatus::transient;
        if (treatUnexpectedErrorsAsFatal)
            return ListenStatus::fatal;
        if (Helper::isAcceptFatalError(ec))
            return ListenStatus::fatal;
        return ListenStatus::transient;
    }
};

//------------------------------------------------------------------------------
using TcpAcceptorConfig = BasicTcpAcceptorConfig<TcpEndpoint>;

//------------------------------------------------------------------------------
using TcpAcceptor = RawsockAcceptor<TcpAcceptorConfig>;

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_INTERNAL_TCPACCEPTOR_HPP
