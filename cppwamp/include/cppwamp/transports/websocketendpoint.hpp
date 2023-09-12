/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WEBSOCKETENDPOINT_HPP
#define CPPWAMP_TRANSPORTS_WEBSOCKETENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Websocket server parameters and
           options. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include "../api.hpp"
#include "tcpprotocol.hpp"
#include "websocketprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains Websocket host address information, as well as other
    socket options. */
//------------------------------------------------------------------------------
class CPPWAMP_API WebsocketEndpoint
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Websocket;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit WebsocketEndpoint(Port port);

    /** Constructor taking an address string and a port number. */
    WebsocketEndpoint(std::string address, unsigned short port);

    /** Specifies the underlying TCP socket options to use. */
    WebsocketEndpoint& withSocketOptions(TcpOptions options);

    /** Specifies the socket options to use on the TCP acceptor socket. */
    WebsocketEndpoint& withAcceptorOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    WebsocketEndpoint& withMaxRxLength(std::size_t length);

    /** Specifies the TCP acceptor's maximum number of pending connections. */
    WebsocketEndpoint& withBacklogCapacity(int capacity);

    /** Obtains the endpoint address. */
    const std::string& address() const;

    /** Obtains the the port number. */
    Port port() const;

    /** Obtains the transport options. */
    const TcpOptions& socketOptions() const;

    /** Obtains the acceptor socket options. */
    const TcpOptions& acceptorOptions() const;

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const;

    /** Obtains the TCP acceptor's maximum number of pending connections. */
    int backlogCapacity() const;

    /** Generates a human-friendly string of the Websocket address/port. */
    std::string label() const;

private:
    std::string address_;
    TcpOptions socketOptions_;
    TcpOptions acceptorOptions_;
    std::size_t maxRxLength_ = 16*1024*1024;
    int backlogCapacity_;
    Port port_ = 0;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/websocketendpoint.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETENDPOINT_HPP
