/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../tcpendpoint.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE TcpEndpoint::TcpEndpoint(unsigned short port, TcpOptions options,
                                        RawsockMaxLength maxRxLength)
    : options_(std::move(options)),
      maxRxLength_(maxRxLength),
      port_(port)
{}

CPPWAMP_INLINE TcpEndpoint::TcpEndpoint(std::string address,
                                        unsigned short port, TcpOptions options,
                                        RawsockMaxLength maxRxLength)
    : address_(std::move(address)),
      options_(std::move(options)),
      maxRxLength_(maxRxLength),
      port_(port)
{}

CPPWAMP_INLINE TcpEndpoint& TcpEndpoint::withOptions(TcpOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE TcpEndpoint& TcpEndpoint::withMaxRxLength(RawsockMaxLength length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE const std::string& TcpEndpoint::address() const
{
    return address_;
}

CPPWAMP_INLINE unsigned short TcpEndpoint::port() const
{
    return port_;
}

CPPWAMP_INLINE const TcpOptions& TcpEndpoint::options() const {return options_;}

CPPWAMP_INLINE RawsockMaxLength TcpEndpoint::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE std::string TcpEndpoint::label() const
{
    if (address_.empty())
        return "TCP Port " + std::to_string(port_);
    return "TCP " + address_ + ':' + std::to_string(port_);
}

} // namespace wamp
