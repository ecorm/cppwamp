/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tcpprotocol.hpp"
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/unicast.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include "../api.hpp"

namespace wamp
{

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)
CPPWAMP_INLINE TcpOptions& TcpOptions::withBroadcast(bool b)          {return set<boost::asio::socket_base::broadcast>(b);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withDebug(bool b)              {return set<boost::asio::socket_base::debug>(b);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withDoNotRoute(bool b)         {return set<boost::asio::socket_base::do_not_route>(b);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withKeepAlive(bool b)          {return set<boost::asio::socket_base::keep_alive>(b);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withLinger(bool b, int n)      {return set<boost::asio::socket_base::linger>(b, n);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withOutOfBandInline(bool b)    {return set<boost::asio::socket_base::out_of_band_inline>(b);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withReceiveBufferSize(int n)   {return set<boost::asio::socket_base::receive_buffer_size>(n);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withReceiveLowWatermark(int n) {return set<boost::asio::socket_base::receive_low_watermark>(n);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withReuseAddress(bool b)       {return set<boost::asio::socket_base::reuse_address>(b);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withSendBufferSize(int n)      {return set<boost::asio::socket_base::send_buffer_size>(n);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withSendLowWatermark(int n)    {return set<boost::asio::socket_base::send_low_watermark>(n);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withUnicastHops(int n)         {return set<boost::asio::ip::unicast::hops>(n);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withIpV6Only(bool b)           {return set<boost::asio::ip::v6_only>(b);}
CPPWAMP_INLINE TcpOptions& TcpOptions::withNoDelay(bool b)            {return set<boost::asio::ip::tcp::no_delay>(b);}
// NOLINTEND(readability-inconsistent-declaration-parameter-name)

template <typename TOption, typename... TArgs>
TcpOptions& TcpOptions::set(TArgs... args)
{
    options_.add(TOption(args...));
    return *this;
}

template <typename TSocket>
void TcpOptions::applyTo(TSocket& socket) const {options_.applyTo(socket);}

// Explicit template instantiation
template void TcpOptions::applyTo(boost::asio::ip::tcp::socket&) const;

} // namespace wamp
