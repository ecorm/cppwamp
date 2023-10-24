/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic Websocket protocol facilities. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <system_error>
#include "../api.hpp"
#include "socketendpoint.hpp"
#include "sockethost.hpp"
#include "tcpprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the Websocket transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Websocket
{
    constexpr Websocket() = default;
};

//------------------------------------------------------------------------------
/** %Error code values used with the WebsocketCloseCategory error category. */
//------------------------------------------------------------------------------
enum class WebsocketCloseErrc
{
    unknown        =    1, /// Websocket connection closed abnormally for unknown reason
    normal         = 1000, /// Websocket connection successfully fulfilled its purpose
    goingAway      = 1001, /// Websocket peer is navigating away or going down
    protocolError  = 1002, /// Websocket protocol error
    unknownData    = 1003, /// Websocket peer cannot accept data type
    badPayload     = 1007, /// Invalid websocket message data type
    policyError    = 1008, /// Websocket peer received a message violating its policy
    tooBig         = 1009, /// Websocket peer received a message too big to process
    needsExtension = 1010, /// Websocket server lacks extension expected by client
    internalError  = 1011, /// Websocket server encountered an unexpected condition
    serviceRestart = 1012, /// Websocket server is restarting
    tryAgainLater  = 1013, /// Websocket connection terminated due to temporary server condition
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting Websocket close reasons.
    @see WebsocketCloseErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API WebsocketCloseCategory : public std::error_category
{
public:
    /** Obtains the name of the category. */
    const char* name() const noexcept override;

    /** Obtains the explanatory string. */
    std::string message(int ev) const override;

    /** Compares `error_code` and and error condition for equivalence. */
    bool equivalent(const std::error_code& code,
                    int condition) const noexcept override;

private:
    CPPWAMP_HIDDEN WebsocketCloseCategory();

    friend WebsocketCloseCategory& websocketCloseCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Websocket
    close reasons.
    @relates WebsocketCloseCategory */
//------------------------------------------------------------------------------
CPPWAMP_API WebsocketCloseCategory& websocketCloseCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a WebsocketCloseErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(WebsocketCloseErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a WebsocketCloseErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(WebsocketCloseErrc errc);


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
    WebsocketHost(std::string address, std::string serviceName);

    /** Constructor taking an URL/IP and a numeric port number. */
    WebsocketHost(std::string address, Port port);

    /** Specifies the request-target (default is "/"). */
    WebsocketHost& withTarget(std::string target);

    /** Specifies the custom agent string to use (default is
        Version::agentString). */
    WebsocketHost& withAgent(std::string agent);

    /** Specifies the maximum duration to wait for the router to complete
        the closing Websocket handshake after an ABORT message is sent. */
    WebsocketHost& withAbortTimeout(Timeout timeout);

    /** Obtains the request-target. */
    const std::string& target() const;

    /** Obtains the custom agent string to use. */
    const std::string& agent() const;

    /** Obtains the Websocket close handshake completion timeout period after
        an ABORT message is sent. */
    Timeout abortTimeout() const;

private:
    using Base = SocketHost<WebsocketHost, Websocket, TcpOptions, std::size_t,
                            16*1024*1024>;

    std::string target_ = "/";
    std::string agent_;
    Timeout abortTimeout_ = unspecifiedTimeout;
};


//------------------------------------------------------------------------------
/** Contains Websocket server address information, as well as other
    socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API WebsocketEndpoint
    : public SocketEndpoint<WebsocketEndpoint, Websocket, TcpOptions>
{
public:
    /** Constructor taking a port number. */
    explicit WebsocketEndpoint(Port port);

    /** Constructor taking an address string and a port number. */
    WebsocketEndpoint(std::string address, unsigned short port);

    /** Specifies the custom agent string to use (default is
        Version::agentString). */
    WebsocketEndpoint& withAgent(std::string agent);

    /** Specifies the total header size limit of HTTP upgrade requests. */
    WebsocketEndpoint& withHttpHeaderLimit(uint32_t limit);

    /** Obtains the custom agent string. */
    const std::string& agent() const;

    /** Generates a human-friendly string of the Websocket address/port. */
    std::string label() const;

    /** Obtains the total header size limit of HTTP upgrade requests. */
    uint32_t httpHeaderLimit() const;

private:
    using Base = SocketEndpoint<WebsocketEndpoint, Websocket, TcpOptions>;

    // Maintenance note: Keep HttpEndpoint::toWebsocket in sync with changes.
    std::string agent_;
    uint32_t httpHeaderLimit_ = 0;
};

} // namespace wamp

//------------------------------------------------------------------------------
#if !defined CPPWAMP_FOR_DOXYGEN
namespace std
{

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::WebsocketCloseErrc>
    : public true_type
{};

} // namespace std
#endif // CPPWAMP_FOR_DOXYGEN

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/websocketprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_WEBSOCKETPROTOCOL_HPP
