/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/udshost.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE UdsHost::UdsHost(std::string pathName)
    : pathName_(std::move(pathName))
{}

CPPWAMP_INLINE UdsHost& UdsHost::withSocketOptions(UdsOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE UdsHost& UdsHost::withMaxRxLength(RawsockMaxLength length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE const std::string& UdsHost::address() const
{
    return pathName_;
}

CPPWAMP_INLINE const UdsOptions& UdsHost::socketOptions() const
{
    return options_;
}

CPPWAMP_INLINE RawsockMaxLength UdsHost::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE std::string UdsHost::label() const
{
    return "Unix domain socket path '" + pathName_ + "'";
}

} // namespace wamp
