/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authorizer.hpp"
#include "../api.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

namespace wamp
{

//******************************************************************************
// Authorization
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(bool allowed) : allowed_(allowed) {}

/** If WampErrc::authorizationDenied, WampErrc::authorizationFailed, or
    WampErrc::discloseMeDisallowed is passed, their corresponding URI shall be
    used in the ERROR message returned to the client. Otherwise, the error
    URI shall be `wamp.error.authorization_failed` and the ERROR message will
    contain two positional arguments:
    - A string formatted as `<ec.category().name()>:<ec.value()`
    - A string containing `ec.message()` */
CPPWAMP_INLINE Authorization::Authorization(std::error_code ec) : error_(ec) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization& Authorization::withDisclosure(DisclosureRule d)
{
    disclosure_ = d;
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::operator bool() const
{
    return !error_ && allowed_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::error_code Authorization::error() const {return error_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Authorization::allowed() const {return allowed_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE DisclosureRule Authorization::disclosure() const
{
    return disclosure_;
}


//******************************************************************************
// AuthorizationRequest
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE const SessionInfo& AuthorizationRequest::info() const
{
    return info_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Topic t, Authorization a)
{
    send(std::move(t), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Pub p, Authorization a)
{
    send(std::move(p), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Procedure p,
                                                    Authorization a)
{
    send(std::move(p), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Rpc r, Authorization a)
{
    send(std::move(r), a);
}

//------------------------------------------------------------------------------
template <typename C>
void AuthorizationRequest::send(C&& command, Authorization a)
{
    auto originator = originator_.lock();
    if (!originator)
        return;
    realm_.processAuthorization(std::move(originator),
                                std::forward<C>(command), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AuthorizationRequest::AuthorizationRequest(
    internal::PassKey, internal::RealmContext r,
    std::shared_ptr<internal::RouterSession> s)
    : realm_(std::move(r)),
      originator_(s),
      info_(s->sharedInfo())
{}


//******************************************************************************
// Authorizer
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Topic t, AuthorizationRequest a)
{
    a.authorize(std::move(t), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Pub p, AuthorizationRequest a)
{
    a.authorize(std::move(p), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Procedure p, AuthorizationRequest a)
{
    a.authorize(std::move(p), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Rpc r, AuthorizationRequest a)
{
    a.authorize(std::move(r), true);
}

} // namespace wamp
