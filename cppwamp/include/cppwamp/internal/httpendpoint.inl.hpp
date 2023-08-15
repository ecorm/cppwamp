/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpendpoint.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(Port port) : port_(port) {}

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(std::string address,
                                        unsigned short port)
    : address_(std::move(address)),
      port_(port)
{}

CPPWAMP_INLINE HttpEndpoint&
HttpEndpoint::withOptions(TcpOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE HttpEndpoint&
HttpEndpoint::withMaxRxLength(std::size_t length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withExactRoute(std::string route,
                                                          AnyHttpAction action)
{
    actionsByExactKey_[std::move(route)] = action;
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withPrefixRoute(
    std::string route, AnyHttpAction action)
{
    actionsByPrefixKey_[std::move(route)] = action;
    return *this;
}

CPPWAMP_INLINE const std::string& HttpEndpoint::address() const
{
    return address_;
}

CPPWAMP_INLINE HttpEndpoint::Port HttpEndpoint::port() const
{
    return port_;
}

CPPWAMP_INLINE const TcpOptions& HttpEndpoint::options() const
{
    return options_;
}

CPPWAMP_INLINE std::size_t HttpEndpoint::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE std::string HttpEndpoint::label() const
{
    if (address_.empty())
        return "HTTP Port " + std::to_string(port_);
    return "HTTP " + address_ + ':' + std::to_string(port_);
}

CPPWAMP_INLINE AnyHttpAction* HttpEndpoint::findAction(const std::string& route)
{
    {
        auto found = actionsByExactKey_.find(route);
        if (found != actionsByExactKey_.end())
            return &(found.value());
    }

    {
        auto found = actionsByPrefixKey_.longest_prefix(route);
        if (found != actionsByPrefixKey_.end())
            return &(found.value());
    }

    return nullptr;
}

} // namespace wamp
