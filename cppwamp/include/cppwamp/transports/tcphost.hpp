/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TCPHOST_HPP
#define CPPWAMP_TRANSPORTS_TCPHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying TCP client transport parameters
           and options. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "tcpprotocol.hpp"
#include "sockethost.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains TCP host address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpHost
    : public SocketHost<TcpHost, Tcp, TcpOptions, RawsockMaxLength,
                        RawsockMaxLength::MB_16>
{
public:
    /** Constructor taking an URL/IP and a service string. */
    TcpHost(std::string address, std::string serviceName)
        : Base(std::move(address), std::move(serviceName))
    {}

    /** Constructor taking an URL/IP and a numeric port number. */
    TcpHost(std::string address, Port port)
        : TcpHost(std::move(address), std::to_string(port))
    {}

private:
    using Base = SocketHost<TcpHost, Tcp, TcpOptions, RawsockMaxLength,
                            RawsockMaxLength::MB_16>;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_TCPHOST_HPP
