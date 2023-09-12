/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocketendpoint.hpp"
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"

namespace wamp
{

CPPWAMP_INLINE WebsocketEndpoint::WebsocketEndpoint(Port port)
    : port_(port)
{
    acceptorOptions_.withReuseAddress(true);
}

CPPWAMP_INLINE WebsocketEndpoint::WebsocketEndpoint(std::string address,
                                                    unsigned short port)
    : address_(std::move(address)),
      port_(port)
{
    acceptorOptions_.withReuseAddress(true);
}

CPPWAMP_INLINE WebsocketEndpoint&
WebsocketEndpoint::withAgent(std::string agent)
{
    agent_ = std::move(agent);
    return *this;
}

CPPWAMP_INLINE WebsocketEndpoint&
WebsocketEndpoint::withSocketOptions(TcpOptions options)
{
    socketOptions_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE WebsocketEndpoint&
WebsocketEndpoint::withAcceptorOptions(TcpOptions options)
{
    acceptorOptions_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE WebsocketEndpoint&
WebsocketEndpoint::withMaxRxLength(std::size_t length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE WebsocketEndpoint&
WebsocketEndpoint::withBacklogCapacity(int capacity)
{
    CPPWAMP_LOGIC_CHECK(capacity >= 0, "Backlog capacity cannot be negative");
    backlogCapacity_ = capacity;
    return *this;
}

CPPWAMP_INLINE const std::string& WebsocketEndpoint::address() const
{
    return address_;
}

CPPWAMP_INLINE WebsocketEndpoint::Port WebsocketEndpoint::port() const
{
    return port_;
}

CPPWAMP_INLINE const std::string& WebsocketEndpoint::agent() const
{
    return agent_;
}

CPPWAMP_INLINE const TcpOptions& WebsocketEndpoint::socketOptions() const
{
    return socketOptions_;
}

CPPWAMP_INLINE const TcpOptions& WebsocketEndpoint::acceptorOptions() const
{
    return acceptorOptions_;
}

CPPWAMP_INLINE std::size_t WebsocketEndpoint::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE int WebsocketEndpoint::backlogCapacity() const
{
    return backlogCapacity_;
}

CPPWAMP_INLINE std::string WebsocketEndpoint::label() const
{
    if (address_.empty())
        return "Websocket Port " + std::to_string(port_);
    return "Websocket " + address_ + ':' + std::to_string(port_);
}

} // namespace wamp
