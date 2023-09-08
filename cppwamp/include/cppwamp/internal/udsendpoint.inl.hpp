/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/udsendpoint.hpp"
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"

namespace wamp
{

CPPWAMP_INLINE UdsEndpoint::UdsEndpoint(std::string pathName)
    : pathName_(std::move(pathName))
{}

CPPWAMP_INLINE UdsEndpoint& UdsEndpoint::withSocketOptions(UdsOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE UdsEndpoint& UdsEndpoint::withAcceptorOptions(UdsOptions options)
{
    acceptorOptions_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE UdsEndpoint&
UdsEndpoint::withMaxRxLength(RawsockMaxLength length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE UdsEndpoint& UdsEndpoint::withDeletePath(bool enabled)
{
    deletePathEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE UdsEndpoint& UdsEndpoint::withBacklogCapacity(int capacity)
{
    CPPWAMP_LOGIC_CHECK(capacity >= 0, "Backlog capacity cannot be negative");
    backlogCapacity_ = capacity;
    return *this;
}

CPPWAMP_INLINE const std::string& UdsEndpoint::pathName() const
{
    return pathName_;
}

CPPWAMP_INLINE const UdsOptions& UdsEndpoint::socketOptions() const
{
    return options_;
}

CPPWAMP_INLINE const UdsOptions& UdsEndpoint::acceptorOptions() const
{
    return acceptorOptions_;
}

CPPWAMP_INLINE RawsockMaxLength UdsEndpoint::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE bool UdsEndpoint::deletePathEnabled() const
{
    return deletePathEnabled_;
}

CPPWAMP_INLINE int UdsEndpoint::backlogCapacity() const {return backlogCapacity_;}

CPPWAMP_INLINE std::string UdsEndpoint::label() const
{
    return "Unix domain socket path '" + pathName_ + "'";
}

} // namespace wamp
