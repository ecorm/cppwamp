/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
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


// Forward declarations
namespace internal
{
class HttpListener;
class TcpOpener;
class WebsocketConnector;
class WebsocketListener;
template <typename> class RawsockAcceptor;
}

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

private:
    template <typename TOption, typename... TArgs>
    TcpOptions& set(TArgs... args);

    template <typename TSocket> void applyTo(TSocket& socket) const;

    internal::SocketOptionList<boost::asio::ip::tcp> options_;

    friend class internal::HttpListener;
    friend class internal::TcpOpener;
    friend class internal::WebsocketConnector;
    friend class internal::WebsocketListener;
    template <typename> friend class internal::RawsockAcceptor;

    /* Implementation note: Explicit template instantiation does not seem
       to play nice with CRTP, so it was not feasible to factor out the
       commonality with UdsOptions as a mixin (not without giving up the
       fluent API). */
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tcpprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TCPPROTOCOL_HPP
