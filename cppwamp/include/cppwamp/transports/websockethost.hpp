/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WEBSOCKETHOST_HPP
#define CPPWAMP_TRANSPORTS_WEBSOCKETHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Websocket transport parameters and
           options. */
//------------------------------------------------------------------------------

#include <string>
#include "../api.hpp"
#include "../connector.hpp"
#include "../timeout.hpp"
#include "tcpprotocol.hpp"
#include "websocketprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains Websocket host address information, as well as other
    socket options.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API WebsocketHost
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Websocket;

    /** Constructor taking an URL/IP and a service string. */
    WebsocketHost(std::string hostName, std::string serviceName);

    /** Constructor taking an URL/IP and a numeric port number. */
    WebsocketHost(std::string hostName, unsigned short port);

    /** Specifies the request-target (default is "/"). */
    WebsocketHost& withTarget(std::string target);

    /** Specifies the custom agent string to use (default is
        Version::agentString). */
    WebsocketHost& withAgent(std::string agent);

    /** Specifies the underlying TCP socket options to use. */
    WebsocketHost& withSocketOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    WebsocketHost& withMaxRxLength(std::size_t length);

    /** Enables keep-alive PING messages with the given interval. */
    WebsocketHost& withHearbeatInterval(Timeout interval);

    /** Specifies the maximum duration to wait for the router to complete
        the closing Websocket handshake after an ABORT message is sent. */
    WebsocketHost& withAbortTimeout(Timeout interval);

    /** Couples a serialization format with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename F, CPPWAMP_NEEDS(IsCodecFormat<F>::value) = 0>
    ConnectionWish withFormat(F) const
    {
        return ConnectionWish{*this, F{}};
    }

    /** Couples serialization format options with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename F>
    ConnectionWish withFormatOptions(const CodecOptions<F>& codecOptions) const
    {
        return ConnectionWish{*this, codecOptions};
    }

    /** Obtains the Websocket host name. */
    const std::string& hostName() const;

    /** Obtains the Websocket service name, or stringified port number. */
    const std::string& serviceName() const;

    /** Obtains the request-target. */
    const std::string& target() const;

    /** Obtains the custom agent string to use. */
    const std::string& agent() const;

    /** Obtains the underlying TCP socket options. */
    const TcpOptions& socketOptions() const;

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const;

    /** Obtains the keep-alive PING message interval. */
    Timeout heartbeatInterval() const;

    /** Obtains the Websocket handshake completion timeout period after
        an ABORT message is sent. */
    Timeout abortTimeout() const;

private:
    std::string hostName_;
    std::string serviceName_;
    std::string target_ = "/";
    std::string agent_;
    TcpOptions socketOptions_;
    Timeout heartbeatInterval_ = unspecifiedTimeout;
    Timeout abortTimeout_ = unspecifiedTimeout;
    std::size_t maxRxLength_ = 16*1024*1024;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/websockethost.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETHOST_HPP
