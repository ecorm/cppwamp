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

#include <cstdint>
#include <string>
#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "tcpprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains TCP server address information, as well as other socket options. */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpEndpoint
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Tcp;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit TcpEndpoint(Port port);

    /** Constructor taking an address string and a port number. */
    TcpEndpoint(std::string address, unsigned short port);

    /** Specifies the socket options to use on the per-peer sockets. */
    TcpEndpoint& withSocketOptions(TcpOptions options);

    /** Specifies the socket options to use on the acceptor socket. */
    TcpEndpoint& withAcceptorOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    TcpEndpoint& withMaxRxLength(RawsockMaxLength length);

    /** Specifies the acceptor's maximum number of pending connections. */
    TcpEndpoint& withBacklogCapacity(int capacity);

    /** Obtains the endpoint address. */
    const std::string& address() const;

    /** Obtains the the port number. */
    Port port() const;

    /** Obtains the per-peer socket options. */
    const TcpOptions& socketOptions() const;

    /** Obtains the acceptor socket options. */
    const TcpOptions& acceptorOptions() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

    /** Obtains the acceptor's maximum number of pending connections. */
    int backlogCapacity() const;

    /** Generates a human-friendly string of the TCP address/port. */
    std::string label() const;

private:
    std::string address_;
    TcpOptions socketOptions_;
    TcpOptions acceptorOptions_;
    int backlogCapacity_;
    RawsockMaxLength maxRxLength_ = RawsockMaxLength::MB_16;
    Port port_ = 0;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tcpendpoint.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TCPENDPOINT_HPP
