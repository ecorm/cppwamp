/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TCPHOST_HPP
#define CPPWAMP_TCPHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying TCP transport parameters and
           options. */
//------------------------------------------------------------------------------

#include <string>
#include "api.hpp"
#include "config.hpp"
#include "connector.hpp"
#include "rawsockoptions.hpp"
#include "tcpprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains TCP host address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpHost
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Tcp;

    /// The default maximum length permitted for incoming messages.
    static constexpr RawsockMaxLength defaultMaxRxLength =
        RawsockMaxLength::MB_16;

    /** Constructor taking a service string. */
    TcpHost(
        std::string hostName,     ///< URL or IP of the router to connect to.
        std::string serviceName,  ///< Service name or stringified port number.
        TcpOptions options = {},  ///< Socket options.
        RawsockMaxLength maxRxLength
            = defaultMaxRxLength  ///< Maximum inbound message length
    );

    /** Constructor taking a numeric port number. */
    TcpHost(
        std::string hostName,     ///< URL or IP of the router to connect to.
        unsigned short port,      ///< Port number.
        TcpOptions options = {},  ///< TCP socket options.
        RawsockMaxLength maxRxLength
            = defaultMaxRxLength  ///< Maximum inbound message length
    );

    /** Specifies the socket options to use. */
    TcpHost& withOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    TcpHost& withMaxRxLength(RawsockMaxLength length);

    /** Couples a serialization format with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename TFormat>
    ConnectionWish withFormat(TFormat) const
    {
        return ConnectionWish{*this, TFormat{}};
    }

    /** Obtains the TCP host name. */
    const std::string& hostName() const;

    /** Obtains the TCP service name, or stringified port number. */
    const std::string& serviceName() const;

    /** Obtains the transport options. */
    const TcpOptions& options() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

    /** The following setters are deprecated. Socket options should
        be passed via the constructor or set via TcpHost::withOptions. */
    /// @{

    /** Adds the SO_BROADCAST socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withBroadcast(bool enabled = true)
    {
        options_.withBroadcast(enabled);
        return *this;
    }

    /** Adds the SO_DEBUG socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withDebug(bool enabled = true)
    {
        options_.withDebug(enabled);
        return *this;
    }

    /** Adds the SO_DONTROUTE socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withDoNotRoute(bool enabled = true)
    {
        options_.withDoNotRoute(enabled);
        return *this;
    }

    /** Adds the SO_KEEPALIVE socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withKeepAlive(bool enabled = true)
    {
        options_.withKeepAlive(enabled);
        return *this;
    }

    /** Adds the SO_LINGER socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withLinger(bool enabled, int timeout)
    {
        options_.withLinger(enabled, timeout);
        return *this;
    }

    /** Adds the SO_OOBINLINE socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withOutOfBandInline(bool enabled)
    {
        options_.withOutOfBandInline(enabled);
        return *this;
    }

    /** Adds the SO_RCVBUF socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withReceiveBufferSize(int size)
    {
        options_.withReceiveBufferSize(size);
        return *this;
    }

    /** Adds the SO_RCVLOWAT socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withReceiveLowWatermark(int size)
    {
        options_.withReceiveLowWatermark(size);
        return *this;
    }

    /** Adds the SO_REUSEADDR socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withReuseAddress(bool enabled = true)
    {
        options_.withReuseAddress(enabled);
        return *this;
    }

    /** Adds the SO_SNDBUF socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withSendBufferSize(int size)
    {
        options_.withSendBufferSize(size);
        return *this;
    }

    /** Adds the SO_SNDLOWAT socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withSendLowWatermark(int size)
    {
        options_.withSendLowWatermark(size);
        return *this;
    }

    /** Adds the IP_UNICAST_TTL socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withUnicastHops(int hops)
    {
        options_.withUnicastHops(hops);
        return *this;
    }

    /** Adds the IP_V6ONLY socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withIpV6Only(bool enabled = true)
    {
        options_.withIpV6Only(enabled);
        return *this;
    }

    /** Adds the TCP_NODELAY socket option. @deprecated */
    CPPWAMP_DEPRECATED TcpHost& withNoDelay(bool enabled = true)
    {
        options_.withNoDelay(enabled);
        return *this;
    }
    /// @}

private:
    std::string hostName_;
    std::string serviceName_;
    TcpOptions options_;
    RawsockMaxLength maxRxLength_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/tcphost.ipp"
#endif

#endif // CPPWAMP_TCPHOST_HPP
