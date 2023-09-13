/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_UDSPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_UDSPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic Unix Domain Socket protocol facilities. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "../internal/socketoptions.hpp"
#include "socketendpoint.hpp"
#include "sockethost.hpp"

// Forward declaration
namespace boost { namespace asio { namespace local { class stream_protocol; }}}

namespace wamp
{

//------------------------------------------------------------------------------
/** Protocol tag type associated with Unix Domain Sockets transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Uds
{
    constexpr Uds() = default;
};


//------------------------------------------------------------------------------
/** Contains options for the UNIX domain socket transport.
    @note Support for these options depends on the the operating system.
          Some may not even make sense for a UNIX domain socket. This library
          aims not to be opinionated about which socket options are irrelevant
          so they are all made available.
    @see https://man7.org/linux/man-pages/man7/socket.7.html
    @see https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt
    @see https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/setsockopt.2.html */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsOptions
{
public:
    // NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)

    /** Adds the SO_BROADCAST socket option. */
    UdsOptions& withBroadcast(bool enabled = true);

    /** Adds the SO_DEBUG socket option. */
    UdsOptions& withDebug(bool enabled = true);

    /** Adds the SO_DONTROUTE socket option. */
    UdsOptions& withDoNotRoute(bool enabled = true);

    /** Adds the SO_KEEPALIVE socket option. */
    UdsOptions& withKeepAlive(bool enabled = true);

    /** Adds the SO_LINGER socket option. */
    UdsOptions& withLinger(bool enabled, int timeout);

    /** Adds the SO_OOBINLINE socket option. */
    UdsOptions& withOutOfBandInline(bool enabled);

    /** Adds the SO_RCVBUF socket option. */
    UdsOptions& withReceiveBufferSize(int size);

    /** Adds the SO_RCVLOWAT socket option. */
    UdsOptions& withReceiveLowWatermark(int size);

    /** Adds the SO_REUSEADDR socket option. */
    UdsOptions& withReuseAddress(bool enabled = true);

    /** Adds the SO_SNDBUF socket option. */
    UdsOptions& withSendBufferSize(int size);

    /** Adds the SO_SNDLOWAT socket option. */
    UdsOptions& withSendLowWatermark(int size);

    // NOLINTEND(readability-inconsistent-declaration-parameter-name)

    /** Applies the options to the given socket. */
    template <typename TSocket> void applyTo(TSocket& socket) const;

private:
    template <typename TOption, typename... TArgs>
    UdsOptions& set(TArgs... args);

    internal::SocketOptionList<boost::asio::local::stream_protocol> options_;

    /* Implementation note: Explicit template instantiation does not seem
       to play nice with CRTP, so it was not feasible to factor out the
       commonality with TcpOptions as a mixin (not without giving up the
       fluent API). */
};


//------------------------------------------------------------------------------
/** Contains a Unix Domain Socket path, as well as other socket options
    for a client connection.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsHost
    : public SocketHost<UdsHost, Uds, UdsOptions, RawsockMaxLength,
                        RawsockMaxLength::MB_16>
{
public:
    /** Constructor taking a path name. */
    explicit UdsHost(std::string pathName);

private:
    using Base = SocketHost<UdsHost, Uds, UdsOptions, RawsockMaxLength,
                            RawsockMaxLength::MB_16>;
    using Base::serviceName;
};


//------------------------------------------------------------------------------
/** Contains a Unix Domain Socket server path, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsEndpoint
    : public SocketEndpoint<UdsEndpoint, Uds, UdsOptions, RawsockMaxLength,
                            RawsockMaxLength::MB_16>
{
public:
    /** Constructor taking a path name. */
    explicit UdsEndpoint(std::string pathName);

    /** Enables/disables the deletion of existing file path before listening. */
    UdsEndpoint& withDeletePath(bool enabled = true);

    /** Returns true if automatic path deletion before listening is enabled. */
    bool deletePathEnabled() const;

    /** Generates a human-friendly string of the UDS path. */
    std::string label() const;

private:
    using Base = SocketEndpoint<UdsEndpoint, Uds, UdsOptions, RawsockMaxLength,
                                RawsockMaxLength::MB_16>;

    using Base::port;

    bool deletePathEnabled_ = true;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/udsprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_UDSPROTOCOL_HPP
