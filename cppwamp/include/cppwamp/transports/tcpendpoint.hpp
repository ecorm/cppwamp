/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TCPENDPOINT_HPP
#define CPPWAMP_TRANSPORTS_TCPENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying TCP server parameters and
           options. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "socketendpoint.hpp"
#include "tcpprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains TCP server address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpEndpoint
    : public SocketEndpoint<TcpEndpoint, Tcp, TcpOptions, RawsockMaxLength,
                            RawsockMaxLength::MB_16>
{
public:
    /** Constructor taking a port number. */
    explicit TcpEndpoint(Port port)
        : Base("", port)
    {
        mutableAcceptorOptions().withReuseAddress(true);
    }

    /** Constructor taking an address string and a port number. */
    TcpEndpoint(std::string address, unsigned short port)
        : Base(std::move(address), port)
    {
        mutableAcceptorOptions().withReuseAddress(true);
    }

    /** Generates a human-friendly string of the TCP address/port. */
    std::string label() const
    {
        auto portString = std::to_string(port());
        if (address().empty())
            return "TCP Port " + portString;
        return "TCP " + address() + ':' + portString;
    }

private:
    using Base = SocketEndpoint<TcpEndpoint, Tcp, TcpOptions, RawsockMaxLength,
                                RawsockMaxLength::MB_16>;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_TCPENDPOINT_HPP
