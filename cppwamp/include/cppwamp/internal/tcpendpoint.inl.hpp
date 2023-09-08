/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tcpendpoint.hpp"
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"

namespace wamp
{

CPPWAMP_INLINE TcpEndpoint::TcpEndpoint(Port port)
    : port_(port)
{
    acceptorOptions_.withReuseAddress(true);
}

CPPWAMP_INLINE TcpEndpoint::TcpEndpoint(std::string address,
                                        unsigned short port)
    : address_(std::move(address)),
      port_(port)
{
    acceptorOptions_.withReuseAddress(true);
}

CPPWAMP_INLINE TcpEndpoint& TcpEndpoint::withSocketOptions(TcpOptions options)
{
    socketOptions_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE TcpEndpoint& TcpEndpoint::withAcceptorOptions(TcpOptions options)
{
    acceptorOptions_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE TcpEndpoint& TcpEndpoint::withMaxRxLength(RawsockMaxLength length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE TcpEndpoint& TcpEndpoint::withBacklogCapacity(int capacity)
{
    CPPWAMP_LOGIC_CHECK(capacity >= 0, "Backlog capacity cannot be negative");
    backlogCapacity_ = capacity;
    return *this;
}

CPPWAMP_INLINE const std::string& TcpEndpoint::address() const
{
    return address_;
}

CPPWAMP_INLINE TcpEndpoint::Port TcpEndpoint::port() const {return port_;}

CPPWAMP_INLINE const TcpOptions& TcpEndpoint::socketOptions() const
{
    return socketOptions_;
}

CPPWAMP_INLINE const TcpOptions& TcpEndpoint::acceptorOptions() const
{
    return acceptorOptions_;
}

CPPWAMP_INLINE RawsockMaxLength TcpEndpoint::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE int TcpEndpoint::backlogCapacity() const
{
    return backlogCapacity_;
}

CPPWAMP_INLINE std::string TcpEndpoint::label() const
{
    if (address_.empty())
        return "TCP Port " + std::to_string(port_);
    return "TCP " + address_ + ':' + std::to_string(port_);
}

} // namespace wamp
