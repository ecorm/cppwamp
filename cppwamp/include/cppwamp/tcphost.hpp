/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TCPHOST_HPP
#define CPPWAMP_TCPHOST_HPP

#include <string>
#include "rawsockoptions.hpp"

// Forward declaration
namespace boost { namespace asio { namespace ip {
class tcp;
}}}

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains TCP host address information, as well as other socket options.
    @see IpOptions, RawsockOptions, connector, legacyConnector */
//------------------------------------------------------------------------------
class TcpHost : public IpOptions<TcpHost, boost::asio::ip::tcp>
{
public:
    /** Constructor taking a host name and service string. */
    TcpHost(
        std::string hostName,   /**< URL or IP of the router to connect to. */
        std::string serviceName /**< Service name or stringified port number. */
    );

    /** Constructor taking a host name and numeric port number. */
    TcpHost(
        std::string hostName,   /**< URL or IP of the router to connect to. */
        unsigned short port     /**< Port number. */
    );

    /** Adds the TCP_NODELAY socket option.
        This option is for disabling the Nagle algorithm. */
    TcpHost& withNoDelay(bool enabled = true);

    /** Obtains the TCP host name. */
    const std::string& hostName() const;

    /** Obtains the TCP service name, or stringified port number. */
    const std::string& serviceName() const;

private:
    using Base = IpOptions<TcpHost, boost::asio::ip::tcp>;

    std::string hostName_;
    std::string serviceName_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/tcphost.ipp"
#endif

#endif // CPPWAMP_TCPHOST_HPP
