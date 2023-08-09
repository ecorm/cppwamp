/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websockethost.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE WebsocketHost::WebsocketHost(std::string hostName,
                                            std::string serviceName)
    : hostName_(std::move(hostName)),
      serviceName_(std::move(serviceName))
{}

CPPWAMP_INLINE WebsocketHost::WebsocketHost(std::string hostName,
                                            unsigned short port)
    : hostName_(std::move(hostName)),
      serviceName_(std::to_string(port))
{}

CPPWAMP_INLINE WebsocketHost& WebsocketHost::withOptions(TcpOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE WebsocketHost& WebsocketHost::withMaxRxLength(std::size_t length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE WebsocketHost&
WebsocketHost::withHearbeatInterval(Timeout interval)
{
    heartbeatInterval_ = interval;
    return *this;
}

CPPWAMP_INLINE const std::string& WebsocketHost::hostName() const
{
    return hostName_;
}

CPPWAMP_INLINE const std::string& WebsocketHost::serviceName() const
{
    return serviceName_;
}

CPPWAMP_INLINE const TcpOptions& WebsocketHost::options() const
{
    return options_;
}

CPPWAMP_INLINE std::size_t WebsocketHost::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE Timeout WebsocketHost::heartbeatInterval() const
{
    return heartbeatInterval_;
}

} // namespace wamp
