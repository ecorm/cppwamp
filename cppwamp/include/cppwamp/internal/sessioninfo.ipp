/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../sessioninfo.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE SessionInfo::SessionInfo() {}

CPPWAMP_INLINE SessionInfo::SessionInfo(String id, String role, String method,
                                        String provider)
    : id_(std::move(id)),
      role_(std::move(role)),
      method_(std::move(method)),
      provider_(std::move(provider))
{}

CPPWAMP_INLINE SessionInfo& SessionInfo::withExtra(Object extra)
{
    extra_ = std::move(extra);
    return *this;
}

CPPWAMP_INLINE SessionInfo& SessionInfo::withNote(any note)
{
    note_ = std::move(note);
    return *this;
}

CPPWAMP_INLINE SessionId SessionInfo::sessionId() const {return sessionId_;}

CPPWAMP_INLINE const Uri& SessionInfo::realmUri() const {return realmUri_;}

CPPWAMP_INLINE const String& SessionInfo::id() const {return id_;}

CPPWAMP_INLINE const String& SessionInfo::role() const {return role_;}

CPPWAMP_INLINE const String& SessionInfo::method() const {return method_;}

CPPWAMP_INLINE const String& SessionInfo::provider() const {return provider_;}

CPPWAMP_INLINE const Object& SessionInfo::extra() const {return extra_;}

CPPWAMP_INLINE const Object& SessionInfo::transport() const {return transport_;}

CPPWAMP_INLINE const any& SessionInfo::note() const {return note_;}

CPPWAMP_INLINE ClientFeatures SessionInfo::features() const {return features_;}

CPPWAMP_INLINE void SessionInfo::reset()
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

CPPWAMP_INLINE void SessionInfo::setId(internal::PassKey, String id)
{
    id_ = std::move(id);
}

CPPWAMP_INLINE void SessionInfo::setSessionId(internal::PassKey, SessionId sid)
{
    sessionId_ = sid;
}

CPPWAMP_INLINE void SessionInfo::setTransport(internal::PassKey,
                                              Object transport)
{
    transport_ = std::move(transport);
}

CPPWAMP_INLINE void SessionInfo::setFeatures(internal::PassKey,
                                             ClientFeatures features)
{
    features_ = features;
}

CPPWAMP_INLINE Object
SessionInfo::join(internal::PassKey, Uri uri, Object routerRoles)
{
    realmUri_ = std::move(uri);

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
    if (!routerRoles.empty())
        details.emplace("roles", std::move(routerRoles));
    extra_.clear();
    return details;
}

} // namespace wamp
