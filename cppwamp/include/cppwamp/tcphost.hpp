/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
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
#include "rawsockoptions.hpp"
#include "internal/socketoptions.hpp"

// Forward declaration
namespace boost { namespace asio { namespace ip { class tcp; }}}

namespace wamp
{

namespace internal { class TcpOpener; } // Forward declaration

//------------------------------------------------------------------------------
/** Contains options for the TCP transport.
    @note Support for these options depends on the the operating system.
    @see https://man7.org/linux/man-pages/man7/socket.7.html
    @see https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt
    @see https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/setsockopt.2.html */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpOptions
{
public:
    /** Adds the SO_BROADCAST socket option. */
    TcpOptions& withBroadcast(bool enabled = true);

    /** Adds the SO_DEBUG socket option. */
    TcpOptions& withDebug(bool enabled = true);

    /** Adds the SO_DONTROUTE socket option. */
    TcpOptions& withDoNotRoute(bool enabled = true);

    /** Adds the SO_KEEPALIVE socket option. */
    TcpOptions& withKeepAlive(bool enabled = true);

    /** Adds the SO_LINGER socket option. */
    TcpOptions& withLinger(bool enabled, int timeout);

    /** Adds the SO_OOBINLINE socket option. */
    TcpOptions& withOutOfBandInline(bool enabled);

    /** Adds the SO_RCVBUF socket option. */
    TcpOptions& withReceiveBufferSize(int size);

    /** Adds the SO_RCVLOWAT socket option. */
    TcpOptions& withReceiveLowWatermark(int size);

    /** Adds the SO_REUSEADDR socket option. */
    TcpOptions& withReuseAddress(bool enabled = true);

    /** Adds the SO_SNDBUF socket option. */
    TcpOptions& withSendBufferSize(int size);

    /** Adds the SO_SNDLOWAT socket option. */
    TcpOptions& withSendLowWatermark(int size);

    /** Adds the IP_UNICAST_TTL socket option. */
    TcpOptions& withUnicastHops(int hops);

    /** Adds the IP_V6ONLY socket option. */
    TcpOptions& withIpV6Only(bool enabled = true);

    /** Adds the TCP_NODELAY socket option.
        This option is for disabling the Nagle algorithm. */
    TcpOptions& withNoDelay(bool enabled = true);

private:
    template <typename TOption, typename... TArgs>
    TcpOptions& set(TArgs... args);

    template <typename TSocket> void applyTo(TSocket& socket) const;

    internal::SocketOptionList<boost::asio::ip::tcp> options_;

    friend class internal::TcpOpener;

    /* Implementation note: Explicit template instantiation does not seem
       to play nice with CRTP, so it was not feasible to factor out the
       commonality with UdsOptions as a mixin (not without giving up the
       fluent API). */
};

//------------------------------------------------------------------------------
/** Contains TCP host address information, as well as other socket options.
    @see connector */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpHost
{
public:
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
    TcpHost& withBroadcast(bool enabled = true)
    {
        options_.withBroadcast(enabled);
        return *this;
    }

    /** Adds the SO_DEBUG socket option. @deprecated */
    TcpHost& withDebug(bool enabled = true)
    {
        options_.withDebug(enabled);
        return *this;
    }

    /** Adds the SO_DONTROUTE socket option. @deprecated */
    TcpHost& withDoNotRoute(bool enabled = true)
    {
        options_.withDoNotRoute(enabled);
        return *this;
    }

    /** Adds the SO_KEEPALIVE socket option. @deprecated */
    TcpHost& withKeepAlive(bool enabled = true)
    {
        options_.withKeepAlive(enabled);
        return *this;
    }

    /** Adds the SO_LINGER socket option. @deprecated */
    TcpHost& withLinger(bool enabled, int timeout)
    {
        options_.withLinger(enabled, timeout);
        return *this;
    }

    /** Adds the SO_OOBINLINE socket option. @deprecated */
    TcpHost& withOutOfBandInline(bool enabled)
    {
        options_.withOutOfBandInline(enabled);
        return *this;
    }

    /** Adds the SO_RCVBUF socket option. @deprecated */
    TcpHost& withReceiveBufferSize(int size)
    {
        options_.withReceiveBufferSize(size);
        return *this;
    }

    /** Adds the SO_RCVLOWAT socket option. @deprecated */
    TcpHost& withReceiveLowWatermark(int size)
    {
        options_.withReceiveLowWatermark(size);
        return *this;
    }

    /** Adds the SO_REUSEADDR socket option. @deprecated */
    TcpHost& withReuseAddress(bool enabled = true)
    {
        options_.withReuseAddress(enabled);
        return *this;
    }

    /** Adds the SO_SNDBUF socket option. @deprecated */
    TcpHost& withSendBufferSize(int size)
    {
        options_.withSendBufferSize(size);
        return *this;
    }

    /** Adds the SO_SNDLOWAT socket option. @deprecated */
    TcpHost& withSendLowWatermark(int size)
    {
        options_.withSendLowWatermark(size);
        return *this;
    }

    /** Adds the IP_UNICAST_TTL socket option. @deprecated */
    TcpHost& withUnicastHops(int hops)
    {
        options_.withUnicastHops(hops);
        return *this;
    }

    /** Adds the IP_V6ONLY socket option. @deprecated */
    TcpHost& withIpV6Only(bool enabled = true)
    {
        options_.withIpV6Only(enabled);
        return *this;
    }

    /** Adds the TCP_NODELAY socket option. @deprecated */
    TcpHost& withNoDelay(bool enabled = true)
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
