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

#include "../api.hpp"
#include "socketendpoint.hpp"
#include "tcpprotocol.hpp"
#include "websocketprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains Websocket host address information, as well as other
    socket options. */
//------------------------------------------------------------------------------
class CPPWAMP_API WebsocketEndpoint
    : public SocketEndpoint<WebsocketEndpoint, Websocket, TcpOptions,
                            std::size_t, 16*1024*1024>
{
public:
    /** Constructor taking a port number. */
    explicit WebsocketEndpoint(Port port)
        : Base("", port)
    {
        mutableAcceptorOptions().withReuseAddress(true);
    }

    /** Constructor taking an address string and a port number. */
    WebsocketEndpoint(std::string address, unsigned short port)
        : Base(std::move(address), port)
    {
        mutableAcceptorOptions().withReuseAddress(true);
    }

    /** Specifies the custom agent string to use (default is
        Version::agentString). */
    WebsocketEndpoint& withAgent(std::string agent)
    {
        agent_ = std::move(agent);
        return *this;
    }

    /** Obtains the custom agent string. */
    const std::string& agent() const {return agent_;}

    /** Generates a human-friendly string of the Websocket address/port. */
    std::string label() const
    {
        auto portString = std::to_string(port());
        if (address().empty())
            return "Websocket Port " + portString;
        return "Websocket " + address() + ':' + portString;
    }

private:
    using Base = SocketEndpoint<WebsocketEndpoint, Websocket, TcpOptions,
                                std::size_t, 16*1024*1024>;

    std::string agent_;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETENDPOINT_HPP
