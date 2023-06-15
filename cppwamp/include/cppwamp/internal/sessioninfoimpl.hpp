/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SESSIONINFOIMPL_HPP
#define CPPWAMP_INTERNAL_SESSIONINFOIMPL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for authentication information. */
//------------------------------------------------------------------------------

#include <memory>
#include "../authinfo.hpp"
#include "../features.hpp"
#include "../wampdefs.hpp"
#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
/** Contains meta-data associated with a WAMP client session. */
//------------------------------------------------------------------------------
class SessionInfoImpl
{
public:
    using Ptr = std::shared_ptr<SessionInfoImpl>;
    using ConstPtr = std::shared_ptr<const SessionInfoImpl>;

    static Ptr create(AuthInfo auth)
    {
        return Ptr(new SessionInfoImpl(std::move(auth)));
    }

    SessionId sessionId() const {return sessionId_;}

    const Uri& realmUri() const {return realmUri_;}

    const AuthInfo& auth() const {return auth_;}

    const Object& transport() const {return transport_;}

    ClientFeatures features() const {return features_;}

    void setSessionId(SessionId sid) {sessionId_ = sid;}

    void setTransport(Object transport) {transport_ = std::move(transport);}

    void setFeatures(ClientFeatures features) {features_ = features;}

    Object join(Uri uri, Object routerRoles = {})
    {
        realmUri_ = std::move(uri);

        auto details = auth_.welcomeDetails({});
        if (!routerRoles.empty())
            details.emplace("roles", std::move(routerRoles));
        return details;
    }

    SessionInfoImpl(const SessionInfoImpl&) = delete;
    SessionInfoImpl& operator=(const SessionInfoImpl&) = delete;

private:
    explicit SessionInfoImpl(AuthInfo&& auth)
        : auth_(std::move(auth))
    {}

    AuthInfo auth_;
    String realmUri_;
    Object transport_;
    ClientFeatures features_;
    SessionId sessionId_ = nullId();
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SESSIONINFOIMPL_HPP
