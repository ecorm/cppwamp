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

CPPWAMP_INLINE SessionId AuthInfo::sessionId() const {return sessionId_;}

CPPWAMP_INLINE const Uri& AuthInfo::realmUri() const {return realmUri_;}

CPPWAMP_INLINE const String& AuthInfo::id() const {return id_;}

CPPWAMP_INLINE const String& AuthInfo::role() const {return role_;}

CPPWAMP_INLINE const String& AuthInfo::method() const {return method_;}

CPPWAMP_INLINE const String& AuthInfo::provider() const {return provider_;}

CPPWAMP_INLINE const Object& AuthInfo::extra() const {return extra_;}

CPPWAMP_INLINE const any& AuthInfo::note() const {return note_;}

CPPWAMP_INLINE void AuthInfo::clear()
{
    realmUri_.clear();
    id_.clear();
    role_.clear();
    method_.clear();
    provider_.clear();
    extra_.clear();
    note_.reset();
    sessionId_ = nullId();
}

CPPWAMP_INLINE Object
AuthInfo::join(internal::PassKey, Uri uri, SessionId sessionId,
               Object routerRoles)
{
    realmUri_ = std::move(uri);
    sessionId_ = sessionId;

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
    details.emplace("roles", std::move(routerRoles));
    extra_.clear();
    return details;
}

} // namespace wamp
