/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../tcphost.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

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
