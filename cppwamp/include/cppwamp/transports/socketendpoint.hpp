/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_SOCKETENDPOINT_HPP
#define CPPWAMP_TRANSPORTS_SOCKETENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying server socket parameters and
           options. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <utility>
#include "../transport.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains server address information, as well as other socket options. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TProcotol, typename TSocketOptions>
class SocketEndpoint
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = TProcotol;

    /// Socket options type
    using SocketOptions = TSocketOptions;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Specifies the socket options to use on the per-peer sockets. */
    TDerived& withSocketOptions(SocketOptions options)
    {
        socketOptions_ = std::move(options);
        return derived();
    }

    /** Specifies the socket options to use on the acceptor socket. */
    TDerived& withAcceptorOptions(SocketOptions options)
    {
        acceptorOptions_ = std::move(options);
        return derived();
    }

    /** Specifies the transport size limits and timeouts. */
    TDerived& withLimits(ServerTransportLimits limits)
    {
        limits_ = limits;
        return derived();
    }

    /** Obtains the endpoint address. */
    const std::string& address() const {return address_;}

    /** Obtains the the port number. */
    Port port() const {return port_;}

    /** Obtains the per-peer socket options. */
    const SocketOptions& socketOptions() const {return socketOptions_;}

    /** Obtains the acceptor socket options. */
    const SocketOptions& acceptorOptions() const {return acceptorOptions_;}

    /** Obtains the transport size limits and timeouts. */
    const ServerTransportLimits& limits() const {return limits_;}

    /** Accesses the transport limits. */
    ServerTransportLimits& limits() {return limits_;}

protected:
    SocketEndpoint(std::string address, unsigned short port)
        : address_(std::move(address)),
          port_(port)
    {}

    SocketOptions& mutableAcceptorOptions() {return acceptorOptions_;}

private:
    TDerived& derived() {return static_cast<TDerived&>(*this);}

    std::string address_;
    SocketOptions socketOptions_;
    SocketOptions acceptorOptions_;
    ServerTransportLimits limits_;
    Port port_ = 0;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_TCPENDPOINT_HPP
