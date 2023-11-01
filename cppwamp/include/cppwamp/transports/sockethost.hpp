/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_SOCKETHOST_HPP
#define CPPWAMP_TRANSPORTS_SOCKETHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying client socket parameters and
           options. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <utility>
#include "../api.hpp"
#include "../connector.hpp"
#include "../timeout.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains socket host address information, as well as other client socket
    options. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TProcotol, typename TSocketOptions,
          typename TLimits>
class CPPWAMP_API SocketHost
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = TProcotol;

    /// Socket options type
    using SocketOptions = TSocketOptions;

    /// Transport limits type
    using Limits = TLimits;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Specifies the socket options to use. */
    TDerived& withSocketOptions(SocketOptions options)
    {
        socketOptions_ = std::move(options);
        return derived();
    }

    /** Enables keep-alive PING messages with the given interval.
        @throw error::Logic if the interval is negative. */
    TDerived& withHearbeatInterval(Timeout interval)
    {
        heartbeatInterval_ = internal::checkTimeout(interval);
        return derived();
    }

    /** Specifies the transport size limits and timeouts. */
    TDerived& withLimits(Limits limits)
    {
        limits_ = limits;
        return derived();
    }

    /** Couples a serialization format with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename F, CPPWAMP_NEEDS(IsCodecFormat<F>::value) = 0>
    ConnectionWish withFormat(F) const
    {
        return ConnectionWish{derived(), F{}};
    }

    /** Couples serialization format options with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename F>
    ConnectionWish withFormatOptions(const CodecOptions<F>& codecOptions) const
    {
        return ConnectionWish{derived(), codecOptions};
    }

    /** Obtains the host name. */
    const std::string& address() const {return address_;}

    /** Obtains the service name, or stringified port number. */
    const std::string& serviceName() const {return serviceName_;}

    /** Obtains the socket options. */
    const SocketOptions& socketOptions() const {return socketOptions_;}

    /** Obtains the keep-alive PING message interval. */
    Timeout heartbeatInterval() const {return heartbeatInterval_;}

    /** Obtains the transport size limits and timeouts. */
    const Limits& limits() const {return limits_;}

    /** Accesses the transport limits. */
    Limits& limits() {return limits_;}

protected:
    SocketHost(std::string address, std::string serviceName)
        : address_(std::move(address)),
          serviceName_(std::move(serviceName))
    {}

private:
    TDerived& derived() {return static_cast<TDerived&>(*this);}

    const TDerived& derived() const
    {
        return static_cast<const TDerived&>(*this);
    }

    std::string address_;
    std::string serviceName_;
    SocketOptions socketOptions_;
    Limits limits_;
    Timeout heartbeatInterval_ = unspecifiedTimeout;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_SOCKETHOST_HPP
