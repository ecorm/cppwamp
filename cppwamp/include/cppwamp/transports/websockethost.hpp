/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WEBSOCKETHOST_HPP
#define CPPWAMP_TRANSPORTS_WEBSOCKETHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Websocket client transport
           parameters and options. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "tcpprotocol.hpp"
#include "websocketprotocol.hpp"
#include "sockethost.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains Websocket host address information, as well as other
    socket options for a client connection.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API WebsocketHost
    : public SocketHost<WebsocketHost, Websocket, TcpOptions, std::size_t,
                        16*1024*1024>
{
public:
    /** Constructor taking an URL/IP and a service string. */
    WebsocketHost(std::string address, std::string serviceName)
        : Base(std::move(address), std::move(serviceName))
    {}

    /** Constructor taking an URL/IP and a numeric port number. */
    WebsocketHost(std::string address, Port port)
        : WebsocketHost(std::move(address), std::to_string(port))
    {}

    /** Specifies the request-target (default is "/"). */
    WebsocketHost& withTarget(std::string target)
    {
        target_ = std::move(target);
        return *this;
    }

    /** Specifies the custom agent string to use (default is
        Version::agentString). */
    WebsocketHost& withAgent(std::string agent)
    {
        agent_ = std::move(agent);
        return *this;
    }

    /** Specifies the maximum duration to wait for the router to complete
        the closing Websocket handshake after an ABORT message is sent. */
    WebsocketHost& withAbortTimeout(Timeout timeout)
    {
        abortTimeout_ = timeout;
        return *this;
    }

    /** Obtains the request-target. */
    const std::string& target() const {return target_;}

    /** Obtains the custom agent string to use. */
    const std::string& agent() const {return agent_;}

    /** Obtains the Websocket handshake completion timeout period after
        an ABORT message is sent. */
    Timeout abortTimeout() const {return abortTimeout_;}

private:
    using Base = SocketHost<WebsocketHost, Websocket, TcpOptions, std::size_t,
                            16*1024*1024>;

    std::string target_ = "/";
    std::string agent_;
    Timeout abortTimeout_ = unspecifiedTimeout;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETHOST_HPP
