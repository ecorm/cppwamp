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

//******************************************************************************
// AuthInfo
//******************************************************************************

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


//******************************************************************************
// SessionInfo
//******************************************************************************

CPPWAMP_INLINE SessionInfo::SessionInfo() {}

CPPWAMP_INLINE SessionId SessionInfo::sessionId() const {return sessionId_;}

CPPWAMP_INLINE const Uri& SessionInfo::realmUri() const {return realmUri_;}

CPPWAMP_INLINE const AuthInfo& SessionInfo::auth() const {return auth_;}

CPPWAMP_INLINE const Object& SessionInfo::transport() const {return transport_;}

CPPWAMP_INLINE ClientFeatures SessionInfo::features() const {return features_;}

CPPWAMP_INLINE SessionInfo::SessionInfo(AuthInfo&& auth)
    : auth_(std::move(auth))
{}

CPPWAMP_INLINE SessionInfo::Ptr SessionInfo::create(internal::PassKey,
                                                    AuthInfo auth)
{
    return Ptr(new SessionInfo(std::move(auth)));
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

CPPWAMP_INLINE Object SessionInfo::join(internal::PassKey, Uri uri,
                                        Object routerRoles)
{
    realmUri_ = std::move(uri);

    auto details = auth_.welcomeDetails({});
    if (!routerRoles.empty())
        details.emplace("roles", std::move(routerRoles));
    return details;
}

} // namespace wamp
