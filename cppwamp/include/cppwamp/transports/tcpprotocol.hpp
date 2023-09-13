/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TCPPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_TCPPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic TCP protocol facilities. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "socketendpoint.hpp"
#include "sockethost.hpp"
#include "../internal/socketoptions.hpp"

// Forward declaration
namespace boost { namespace asio { namespace ip { class tcp; }}}

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the TCP transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Tcp
{
    constexpr Tcp() = default;
};


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
    // NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)

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

    // NOLINTEND(readability-inconsistent-declaration-parameter-name)

    /** Applies the options to the given socket. */
    template <typename TSocket> void applyTo(TSocket& socket) const;

private:
    template <typename TOption, typename... TArgs>
    TcpOptions& set(TArgs... args);


    internal::SocketOptionList<boost::asio::ip::tcp> options_;

    /* Implementation note: Explicit template instantiation does not seem
       to play nice with CRTP, so it was not feasible to factor out the
       commonality with UdsOptions as a mixin (not without giving up the
       fluent API). */
};


//------------------------------------------------------------------------------
/** Contains TCP host address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpHost
    : public SocketHost<TcpHost, Tcp, TcpOptions, RawsockMaxLength,
                        RawsockMaxLength::MB_16>
{
public:
    /** Constructor taking an URL/IP and a service string. */
    TcpHost(std::string address, std::string serviceName);

    /** Constructor taking an URL/IP and a numeric port number. */
    TcpHost(std::string address, Port port);

private:
    using Base = SocketHost<TcpHost, Tcp, TcpOptions, RawsockMaxLength,
                            RawsockMaxLength::MB_16>;
};


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
    explicit TcpEndpoint(Port port);

    /** Constructor taking an address string and a port number. */
    TcpEndpoint(std::string address, unsigned short port);

    /** Generates a human-friendly string of the TCP address/port. */
    std::string label() const;

private:
    using Base = SocketEndpoint<TcpEndpoint, Tcp, TcpOptions, RawsockMaxLength,
                                RawsockMaxLength::MB_16>;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tcpprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TCPPROTOCOL_HPP
