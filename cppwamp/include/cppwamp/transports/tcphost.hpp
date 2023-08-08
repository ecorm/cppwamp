/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TCPHOST_HPP
#define CPPWAMP_TRANSPORTS_TCPHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying TCP transport parameters and
           options. */
//------------------------------------------------------------------------------

#include <string>
#include "../api.hpp"
#include "../connector.hpp"
#include "../rawsockoptions.hpp"
#include "../timeout.hpp"
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

    /** Constructor taking an URL/IP and a service string. */
    TcpHost(std::string hostName, std::string serviceName);

    /** Constructor taking an URL/IP and a numeric port number. */
    TcpHost(std::string hostName, unsigned short port);

    /** Specifies the socket options to use. */
    TcpHost& withOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    TcpHost& withMaxRxLength(RawsockMaxLength length);

    /** Enables keep-alive PING messages with the given interval. */
    TcpHost& withHearbeatInterval(Timeout interval);

    /** Couples a serialization format with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename F, CPPWAMP_NEEDS(IsCodecFormat<F>::value) = 0>
    ConnectionWish withFormat(F) const
    {
        return ConnectionWish{*this, F{}};
    }

    /** Couples serialization format options with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename F>
    ConnectionWish withFormatOptions(const CodecOptions<F>& codecOptions) const
    {
        return ConnectionWish{*this, codecOptions};
    }

    /** Obtains the TCP host name. */
    const std::string& hostName() const;

    /** Obtains the TCP service name, or stringified port number. */
    const std::string& serviceName() const;

    /** Obtains the transport options. */
    const TcpOptions& options() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

    /** Obtains the keep-alive PING message interval. */
    Timeout heartbeatInterval() const;

private:
    std::string hostName_;
    std::string serviceName_;
    TcpOptions options_;
    Timeout heartbeatInterval_ = unspecifiedTimeout;
    RawsockMaxLength maxRxLength_ = RawsockMaxLength::MB_16;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tcphost.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TCPHOST_HPP
