/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../sessioninfo.hpp"
#include <utility>
#include "../api.hpp"
#include "sessioninfoimpl.hpp"

namespace wamp
{

CPPWAMP_INLINE SessionInfo::SessionInfo() = default;

CPPWAMP_INLINE SessionId SessionInfo::sessionId() const
{
    return impl_ ? impl_->sessionId() : 0;
}

CPPWAMP_INLINE const Uri& SessionInfo::realmUri() const
{
    static const Uri empty;
    return impl_ ? impl_->realmUri() : empty;
}

CPPWAMP_INLINE const AuthInfo& SessionInfo::auth() const
{
    static const AuthInfo empty;
    return impl_ ? impl_->auth() : empty;
}

CPPWAMP_INLINE ConnectionInfo SessionInfo::connection() const
{
    return impl_ ? impl_->connection() : ConnectionInfo{};
}

CPPWAMP_INLINE const String& SessionInfo::agent() const
{
    static const String empty;
    return impl_ ? impl_->agent() : empty;
}

CPPWAMP_INLINE ClientFeatures SessionInfo::features() const
{
    return impl_ ? impl_->features() : ClientFeatures{};
}

CPPWAMP_INLINE SessionInfo::operator bool() const {return bool(impl_);}

CPPWAMP_INLINE SessionInfo::SessionInfo(
    internal::PassKey, std::shared_ptr<const internal::SessionInfoImpl> impl)
    : impl_(std::move(impl))
{}

} // namespace wamp
