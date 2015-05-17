/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include <boost/asio/ip/tcp.hpp>
#include "config.hpp"

namespace wamp
{

CPPWAMP_INLINE TcpHost::TcpHost(std::string hostName, std::string serviceName)
    : hostName_(std::move(hostName)),
      serviceName_(std::move(serviceName))
{}

CPPWAMP_INLINE TcpHost::TcpHost(std::string hostName, unsigned short port)
    : hostName_(std::move(hostName)),
      serviceName_(std::to_string(port))
{}

CPPWAMP_INLINE TcpHost& TcpHost::withNoDelay(bool enabled)
{
    return addOption(boost::asio::ip::tcp::no_delay(enabled));
}

CPPWAMP_INLINE const std::string& TcpHost::hostName() const
{
    return hostName_;
}

CPPWAMP_INLINE const std::string& TcpHost::serviceName() const
{
    return serviceName_;
}

} // namespace wamp
