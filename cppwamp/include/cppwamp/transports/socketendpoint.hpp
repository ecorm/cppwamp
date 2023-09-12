/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_SOCKETENDPOINT_HPP
#define CPPWAMP_TRANSPORTS_SOCKETENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying server parameters and options. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include "../exceptions.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains server address information, as well as other socket options. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TProcotol, typename TSocketOptions,
          typename TRxLength, TRxLength defaultMaxRxLength>
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

    /** Specifies the maximum length permitted for incoming messages. */
    TDerived& withMaxRxLength(TRxLength length)
    {
        maxRxLength_ = length;
        return derived();
    }

    /** Specifies the acceptor's maximum number of pending connections.
        A value of zero will make the acceptor use the default backlog
        capacity.
        @throws error::Logic if the given capacity is negative. */
    TDerived& withBacklogCapacity(int capacity)
    {
        CPPWAMP_LOGIC_CHECK(capacity >= 0,
                            "Backlog capacity cannot be negative");
        backlogCapacity_ = capacity;
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

    /** Obtains the specified maximum incoming message length. */
    TRxLength maxRxLength() const {return maxRxLength_;}

    /** Obtains the acceptor's maximum number of pending connections. */
    int backlogCapacity() const {return backlogCapacity_;}

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
    int backlogCapacity_;
    TRxLength maxRxLength_ = defaultMaxRxLength;
    Port port_ = 0;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_TCPENDPOINT_HPP
