/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpendpoint.hpp"
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"

namespace wamp
{

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(Port port) : port_(port) {}

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(std::string address,
                                        unsigned short port)
    : address_(std::move(address)),
      port_(port)
{}

CPPWAMP_INLINE HttpEndpoint&
HttpEndpoint::withSocketOptions(TcpOptions options)
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

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withExactRoute(std::string uri,
                                                          AnyHttpAction action)
{
    actionsByExactKey_[std::move(uri)] = action;
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withPrefixRoute(std::string uri,
                                                           AnyHttpAction action)
{
    actionsByPrefixKey_[std::move(uri)] = action;
    return *this;
}

/** @pre `static_cast<unsigned>(status) >= 300` */
CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withErrorPage(HttpStatus status,
                                                         std::string uri)
{
    CPPWAMP_LOGIC_CHECK(static_cast<unsigned>(status) >= 300,
                        "'status' must be a redirect or error code");
    return withErrorPage(status, std::move(uri), status);
}

/** @pre `static_cast<unsigned>(status) >= 300` */
CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withErrorPage(
    HttpStatus status, std::string uri, HttpStatus changedStatus)
{
    CPPWAMP_LOGIC_CHECK(static_cast<unsigned>(status) >= 300,
                        "'status' must be a redirect or error code");
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

CPPWAMP_INLINE const HttpEndpoint::ErrorPage*
HttpEndpoint::findErrorPage(HttpStatus status) const
{
    auto found = errorPages_.find(status);
    return found == errorPages_.end() ? nullptr : &(found->second);
}

CPPWAMP_INLINE AnyHttpAction* HttpEndpoint::doFindAction(const char* route)
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
