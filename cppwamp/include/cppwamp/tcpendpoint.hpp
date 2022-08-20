/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TCPENDPOINT_HPP
#define CPPWAMP_TCPENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying TCP server parameters and
           options. */
//------------------------------------------------------------------------------

#include <string>
#include "api.hpp"
#include "rawsockoptions.hpp"
#include "tcpprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains TCP host address information, as well as other socket options. */
//------------------------------------------------------------------------------
class CPPWAMP_API TcpEndpoint
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Tcp;

    /// The default maximum length permitted for incoming messages.
    static constexpr RawsockMaxLength defaultMaxRxLength =
        RawsockMaxLength::MB_16;

    /** Constructor taking a port number. */
    TcpEndpoint(
        unsigned short port,         ///< Port number.
        TcpOptions options = {},     ///< TCP socket options.
        RawsockMaxLength maxRxLength
            = defaultMaxRxLength     ///< Maximum inbound message length
        );

    /** Constructor taking an address string and a port number. */
    TcpEndpoint(
        std::string address,         ///< Address string.
        unsigned short port,         ///< Port number.
        TcpOptions options = {},     ///< TCP socket options.
        RawsockMaxLength maxRxLength
            = defaultMaxRxLength     ///< Maximum inbound message length
    );

    /** Specifies the socket options to use. */
    TcpEndpoint& withOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    TcpEndpoint& withMaxRxLength(RawsockMaxLength length);

    /** Obtains the endpoint address. */
    const std::string& address() const;

    /** Obtains the the port number. */
    unsigned short port() const;

    /** Obtains the transport options. */
    const TcpOptions& options() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

private:
    std::string address_;
    TcpOptions options_;
    RawsockMaxLength maxRxLength_;
    unsigned short port_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/tcpendpoint.ipp"
#endif

#endif // CPPWAMP_TCPENDPOINT_HPP
