/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authinfo.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE AuthInfo::AuthInfo() {}

CPPWAMP_INLINE AuthInfo::AuthInfo(String id, String role, String method,
                                  String provider)
    : id_(std::move(id)),
      role_(std::move(role)),
      method_(std::move(method)),
      provider_(std::move(provider))
{}

CPPWAMP_INLINE AuthInfo& AuthInfo::withExtra(Object extra)
{
    extra_ = std::move(extra);
    return *this;
}

CPPWAMP_INLINE AuthInfo& AuthInfo::withNote(any note)
{
    note_ = std::move(note);
    return *this;
}

CPPWAMP_INLINE const String& AuthInfo::id() const {return id_;}

CPPWAMP_INLINE const String& AuthInfo::role() const {return role_;}

CPPWAMP_INLINE const String& AuthInfo::method() const {return method_;}

CPPWAMP_INLINE const String& AuthInfo::provider() const {return provider_;}

CPPWAMP_INLINE Object AuthInfo::welcomeDetails(internal::PassKey)
{
    Object details;
    if (!id_.empty())
        details.emplace("authid", id_);
    if (!role_.empty())
        details.emplace("authrole", role_);
    if (!method_.empty())
        details.emplace("authmethod", method_);
    if (!provider_.empty())
        details.emplace("authprovider", provider_);
    if (!extra_.empty())
        details.emplace("authextra", std::move(extra_));
    extra_.clear();
    return details;
}

CPPWAMP_INLINE void AuthInfo::setId(internal::PassKey, String id)
{
    id_ = std::move(id);
}

} // namespace wamp
