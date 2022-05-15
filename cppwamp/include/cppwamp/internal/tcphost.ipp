/*------------------------------------------------------------------------------
               Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../tcphost.hpp"
#include <utility>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/unicast.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// TcpOptions
//******************************************************************************

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


//******************************************************************************
// TcpHost
//******************************************************************************

CPPWAMP_INLINE TcpHost::TcpHost(std::string hostName, std::string serviceName,
                                TcpOptions options,
                                RawsockMaxLength maxRxLength)
    : hostName_(std::move(hostName)),
      serviceName_(std::move(serviceName)),
      options_(std::move(options)),
      maxRxLength_(maxRxLength)
{}

CPPWAMP_INLINE TcpHost::TcpHost(std::string hostName, unsigned short port,
                                TcpOptions options,
                                RawsockMaxLength maxRxLength)
    : hostName_(std::move(hostName)),
      serviceName_(std::to_string(port)),
      options_(std::move(options)),
      maxRxLength_(maxRxLength)
{}

CPPWAMP_INLINE TcpHost& TcpHost::withOptions(TcpOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE TcpHost& TcpHost::withMaxRxLength(RawsockMaxLength length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE const std::string& TcpHost::hostName() const
{
    return hostName_;
}

CPPWAMP_INLINE const std::string& TcpHost::serviceName() const
{
    return serviceName_;
}

CPPWAMP_INLINE const TcpOptions& TcpHost::options() const {return options_;}

CPPWAMP_INLINE RawsockMaxLength TcpHost::maxRxLength() const
{
    return maxRxLength_;
}

} // namespace wamp
