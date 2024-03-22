/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_WSSPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_WSSPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic Websocket Secure (wss) protocol facilities. */
//------------------------------------------------------------------------------

#include <memory>
#include "sslcontext.hpp"
#include "websocketprotocol.hpp"
#include "../internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the Websocket Secure transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Wss
{
    constexpr Wss() = default;
};


//------------------------------------------------------------------------------
/** Contains Websocket Secure host address information, as well as other
    socket options for a client connection.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API WssHost
    : public SocketHost<WssHost, Wss, TcpOptions, WebsocketClientLimits>
{
public:
    /** Determines if the given request-target is valid. */
    static bool targetIsValid(const std::string& target);

    /** Constructor taking an URL/IP and a service string. */
    WssHost(std::string address, std::string serviceName,
            SslContextGenerator generator);

    /** Constructor taking an URL/IP and a numeric port number. */
    WssHost(std::string address, Port port, SslContextGenerator generator);

    /** Specifies the request-target (default is "/"). */
    WssHost& withTarget(std::string target);

    /** Specifies the Websocket options. */
    WssHost& withOptions(WebsocketOptions options);

    /** Specifies the SSL peer verification options. */
    WssHost& withSslVerifyOptions(SslVerifyOptions options);

    /** Obtains the request-target. */
    const std::string& target() const;

    /** Obtains the Websocket options. */
    const WebsocketOptions& options() const;

    /** Obtains the SSL peer verification options. */
    const SslVerifyOptions& sslVerifyOptions() const;

private:
    using Base = SocketHost<WssHost, Wss, TcpOptions, WebsocketClientLimits>;

    std::string target_;
    WebsocketOptions options_;
    SslContextGenerator sslContextGenerator_;
    SslVerifyOptions sslVerifyOptions_;

public: // Internal use only
    ErrorOr<SslContext> makeSslContext(internal::PassKey) const;
};


//------------------------------------------------------------------------------
/** Contains Websocket Secure server address information, as well as other
    socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API WssEndpoint
    : public SocketEndpoint<WssEndpoint, Wss, TcpOptions,
                            WebsocketServerLimits>
{
public:
    /** Constructor taking a port number. */
    explicit WssEndpoint(Port port, SslContextGenerator generator);

    /** Constructor taking an address string and a port number. */
    WssEndpoint(std::string address, Port port, SslContextGenerator generator);

    /** Specifies the Websocket options. */
    WssEndpoint& withOptions(WebsocketOptions options);

    /** Obtains the Websocket options. */
    const WebsocketOptions& options() const;

    /** Generates a human-friendly string of the Websocket address/port. */
    std::string label() const;

private:
    using Base = SocketEndpoint<WssEndpoint, Wss, TcpOptions,
                                WebsocketServerLimits>;

    WebsocketOptions options_;
    SslContextGenerator sslContextGenerator_;

public: // Internal use only
    void initialize(internal::PassKey);
    ErrorOr<SslContext> makeSslContext(internal::PassKey) const;

    template <typename THttpSettings>
    static std::shared_ptr<WssEndpoint> fromHttp(internal::PassKey,
                                                 const THttpSettings& s)
    {
        return std::make_shared<WssEndpoint>(s.address(), s.port(), nullptr);
    }
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/wssprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_WSSPROTOCOL_HPP
