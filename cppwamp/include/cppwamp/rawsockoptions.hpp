/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_RAWSOCKOPTIONS_HPP
#define CPPWAMP_RAWSOCKOPTIONS_HPP

#include "asiodefs.hpp"
#include "./internal/socketoptions.hpp"

namespace wamp
{

namespace internal
{

template <typename TRawsockOptions, typename TSocket>
void applyRawsockOptions(const TRawsockOptions& options, TSocket& socket);

} // namespace internal


//------------------------------------------------------------------------------
/** Enumerators used to specify the maximum length of messages that a raw
    socket transport can receive. */
//------------------------------------------------------------------------------
enum class RawsockMaxLength
{
    B_512,  ///< 512 bytes
    kB_1,   ///< 1 kilobyte
    kB_2,   ///< 2 kilobytes
    kB_4,   ///< 4 kilobytes
    kB_8,   ///< 8 kilobytes
    kB_16,  ///< 16 kilobytes
    kB_32,  ///< 32 kilobytes
    kB_64,  ///< 64 kilobytes
    kB_128, ///< 128 kilobytes
    kB_256, ///< 256 kilobytes
    kB_512, ///< 512 kilobytes
    MB_1,   ///< 1 megabyte
    MB_2,   ///< 2 megabytes
    MB_4,   ///< 4 megabytes
    MB_8,   ///< 8 megabytes
    MB_16   ///< 16 megabytes
};


//------------------------------------------------------------------------------
/** Base class containing general options for raw socket connections.
    @note Support for these options depends on the socket protocol, as well as
          the operating system. For example `withDoNotRoute` probably has no
          effect on Unix domain sockets.
    @tparam TDerived Type of the class that derives from this one. Used for
                     method chaining.
    @tparam TProtocol The Boost Asio protocol type these options apply to. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TProtocol>
class RawsockOptions
{
public:
    /// The default maximum length permitted for incoming messages.
    static constexpr RawsockMaxLength defaultMaxRxLength =
            RawsockMaxLength::MB_16;

    /** Specifies the maximum length permitted for incoming messages. */
    TDerived& withMaxRxLength(RawsockMaxLength length);

    /** Adds the SO_BROADCAST socket option. */
    TDerived& withBroadcast(bool enabled = true);

    /** Adds the SO_DEBUG socket option. */
    TDerived& withDebug(bool enabled = true);

    /** Adds the SO_DONTROUTE socket option. */
    TDerived& withDoNotRoute(bool enabled = true);

    /** Adds the SO_KEEPALIVE socket option. */
    TDerived& withKeepAlive(bool enabled = true);

    /** Adds the SO_LINGER socket option. */
    TDerived& withLinger(bool enabled, int timeout);

    /** Adds the SO_RCVBUF socket option. */
    TDerived& withReceiveBufferSize(int size);

    /** Adds the SO_RCVLOWAT socket option. */
    TDerived& withReceiveLowWatermark(int size);

    /** Adds the SO_REUSEADDR socket option. */
    TDerived& withReuseAddress(bool enabled = true);

    /** Adds the SO_SNDBUF socket option. */
    TDerived& withSendBufferSize(int size);

    /** Adds the SO_SNDLOWAT socket option. */
    TDerived& withSendLowWatermark(int size);

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

protected:
    RawsockOptions();

    template <typename TOption>
    TDerived& addOption(TOption&& opt)
    {
        socketOptions_.add(std::forward<TOption>(opt));
        return static_cast<TDerived&>(*this);
    }

private:
    RawsockMaxLength maxRxLength_;
    internal::SocketOptionList<TProtocol> socketOptions_;

    template <typename TRawsockOptions, typename TSocket>
    friend void internal::applyRawsockOptions(const TRawsockOptions& options,
                                              TSocket& socket);
};

//------------------------------------------------------------------------------
/** Base class containing options for IP-based raw socket connections.
    @tparam TDerived Type of the class that derives from this one. Used for
                     method chaining.
    @tparam TProtocol The Boost Asio protocol type these options apply to. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TProtocol>
class IpOptions : public RawsockOptions<TDerived, TProtocol>
{
public:
    /** Adds the IP_UNICAST_TTL socket option. */
    TDerived& withUnicastHops(int hops);

    /** Adds the IP_V6ONLY socket option. */
    TDerived& withIpV6Only(bool enabled = true);

protected:
    IpOptions();

private:
    using Base = RawsockOptions<TDerived, TProtocol>;
};


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/rawsockoptions.ipp"
#endif

#endif // CPPWAMP_RAWSOCKOPTIONS_HPP
