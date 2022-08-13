/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UDSPATH_HPP
#define CPPWAMP_UDSPATH_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Unix domain socket
           transport parameters and options. */
//------------------------------------------------------------------------------

#include <string>
#include "api.hpp"
#include "rawsockoptions.hpp"
#include "internal/socketoptions.hpp"

// Forward declaration
namespace boost { namespace asio { namespace local { class stream_protocol; }}}

namespace wamp
{

namespace internal { class UdsOpener; } // Forward declaration

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

private:
    template <typename TOption, typename... TArgs>
    UdsOptions& set(TArgs... args);

    template <typename TSocket> void applyTo(TSocket& socket) const;

    internal::SocketOptionList<boost::asio::local::stream_protocol> options_;

    friend class internal::UdsOpener;

    /* Implementation note: Explicit template instantiation does not seem
       to play nice with CRTP, so it was not feasible to factor out the
       commonality with TcpOptions as a mixin (not without giving up the
       fluent API). */
};

//------------------------------------------------------------------------------
/** Contains a Unix domain socket path, as well as other socket options.
    @see RawsockOptions, connector */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsPath
{
public:
    /// The default maximum length permitted for incoming messages.
    static constexpr RawsockMaxLength defaultMaxRxLength =
        RawsockMaxLength::MB_16;

    /** Converting constructor taking a path name. */
    UdsPath(
        std::string pathName,        ///< Path name of the Unix domain socket.
        UdsOptions options = {},     ///< Socket options.
        RawsockMaxLength maxRxLength
        = defaultMaxRxLength         ///< Maximum inbound message length
    );

    /** Specifies the socket options to use. */
    UdsPath& withOptions(UdsOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    UdsPath& withMaxRxLength(RawsockMaxLength length);

    /** Obtains the path name. */
    const std::string& pathName() const;

    /** Obtains the transport options. */
    const UdsOptions& options() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

    /** The following setters are deprecated. Socket options should
        be passed via the constructor or set via UdsPath::withOptions. */
    /// @{

    /** Adds the SO_BROADCAST socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withBroadcast(bool enabled = true)
    {
        options_.withBroadcast(enabled);
        return *this;
    }

    /** Adds the SO_DEBUG socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withDebug(bool enabled = true)
    {
        options_.withDebug(enabled);
        return *this;
    }

    /** Adds the SO_DONTROUTE socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withDoNotRoute(bool enabled = true)
    {
        options_.withDoNotRoute(enabled);
        return *this;
    }

    /** Adds the SO_KEEPALIVE socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withKeepAlive(bool enabled = true)
    {
        options_.withKeepAlive(enabled);
        return *this;
    }

    /** Adds the SO_OOBINLINE socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withOutOfBandInline(bool enabled)
    {
        options_.withOutOfBandInline(enabled);
        return *this;
    }

    /** Adds the SO_LINGER socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withLinger(bool enabled, int timeout)
    {
        options_.withLinger(enabled, timeout);
        return *this;
    }

    /** Adds the SO_RCVBUF socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withReceiveBufferSize(int size)
    {
        options_.withReceiveBufferSize(size);
        return *this;
    }

    /** Adds the SO_RCVLOWAT socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withReceiveLowWatermark(int size)
    {
        options_.withReceiveLowWatermark(size);
        return *this;
    }

    /** Adds the SO_REUSEADDR socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withReuseAddress(bool enabled = true)
    {
        options_.withReuseAddress(enabled);
        return *this;
    }

    /** Adds the SO_SNDBUF socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withSendBufferSize(int size)
    {
        options_.withSendBufferSize(size);
        return *this;
    }

    /** Adds the SO_SNDLOWAT socket option. @deprecated */
    CPPWAMP_DEPRECATED UdsPath& withSendLowWatermark(int size)
    {
        options_.withSendLowWatermark(size);
        return *this;
    }
    /// @}

private:
    std::string pathName_;
    UdsOptions options_;
    RawsockMaxLength maxRxLength_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/udspath.ipp"
#endif

#endif // CPPWAMP_UDSPATH_HPP
