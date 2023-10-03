/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../connectioninfo.hpp"
#include <utility>
#include "../api.hpp"
#include "connectioninfoimpl.hpp"

namespace wamp
{

CPPWAMP_INLINE ConnectionInfo::ConnectionInfo() = default;

CPPWAMP_INLINE ConnectionInfo::ConnectionInfo(
    Object transport, std::string endpoint, const std::string& server)
    : impl_(std::make_shared<internal::ConnectionInfoImpl>(
        std::move(transport), std::move(endpoint), server))
{}

CPPWAMP_INLINE const Object& ConnectionInfo::transport() const
{
    static const Object empty;
    return impl_ ? impl_->transport() : empty;
}

CPPWAMP_INLINE const std::string& ConnectionInfo::endpoint() const
{
    static const std::string empty;
    return impl_ ? impl_->endpoint() : empty;
}

CPPWAMP_INLINE const std::string& ConnectionInfo::server() const
{
    static const std::string empty;
    return impl_ ? impl_->server() : empty;
}

CPPWAMP_INLINE ConnectionInfo::ServerSessionNumber
ConnectionInfo::serverSessionNumber() const
{
    return impl_ ? impl_->serverSessionNumber() : 0;
}

CPPWAMP_INLINE ConnectionInfo::operator bool() const
{
    return static_cast<bool>(impl_);
}

CPPWAMP_INLINE ConnectionInfo::ConnectionInfo(
    internal::PassKey, std::shared_ptr<internal::ConnectionInfoImpl> impl)
    : impl_(std::move(impl))
{}

CPPWAMP_INLINE void ConnectionInfo::setServerSessionNumber(
    internal::PassKey, ServerSessionNumber n)
{
    impl_->setServerSessionNumber(n);
}

} // namespace wamp
