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
