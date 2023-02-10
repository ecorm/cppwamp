/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authorizer.hpp"
#include "../api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(bool allowed) : allowed_(allowed) {}

CPPWAMP_INLINE Authorization::Authorization(std::error_code ec) : error_(ec) {}

CPPWAMP_INLINE Authorization& Authorization::withTrustLevel(TrustLevel tl)
{
    trustLevel_ = tl;
    return *this;
}

CPPWAMP_INLINE Authorization&
Authorization::withDisclosure(DisclosureRule d)
{
    disclosure_ = d;
    return *this;
}

CPPWAMP_INLINE std::error_code Authorization::error() const {return error_;}

CPPWAMP_INLINE bool Authorization::allowed() const {return allowed_;}

CPPWAMP_INLINE bool Authorization::hasTrustLevel() const
{
    return trustLevel_ >= 0;
}

CPPWAMP_INLINE TrustLevel Authorization::trustLevel() const
{
    return trustLevel_;
}

CPPWAMP_INLINE DisclosureRule Authorization::disclosure() const
{
    return disclosure_;
}


//------------------------------------------------------------------------------
CPPWAMP_INLINE AuthorizationAction AuthorizationRequest::action() const
{
    return action_;
}

CPPWAMP_INLINE const AuthInfo& AuthorizationRequest::authInfo() const
{
    return *authInfo_;
}

/** @pre this->action == AuthorizationAction::publish.
    @throws bad_any_cast if the precondition is not met. */
CPPWAMP_INLINE const Pub& AuthorizationRequest::pub() const
{
    return dataAs<Pub>();
}

/** @pre this->action == AuthorizationAction::subscribe.
    @throws bad_any_cast if the precondition is not met. */
CPPWAMP_INLINE const Topic& AuthorizationRequest::topic() const
{
    return dataAs<Topic>();
}

/** @pre this->action == AuthorizationAction::enroll.
    @throws bad_any_cast if the precondition is not met. */
CPPWAMP_INLINE const Procedure& AuthorizationRequest::procedure() const
{
    return dataAs<Procedure>();
}

/** @pre this->action == AuthorizationAction::call.
    @throws bad_any_cast if the precondition is not met. */
CPPWAMP_INLINE const Rpc& AuthorizationRequest::rpc() const
{
    return dataAs<Rpc>();
}

/** @tparam T Either Pub, Topic, Procedure, or Rpc
    @throws bad_any_cast if T does not correspond to the operation data type. */
CPPWAMP_INLINE void AuthorizationRequest::authorize(Authorization a)
{
    CPPWAMP_LOGIC_CHECK(!completed_,
                        "wamp::AuthorizationRequest already completed");
    handler_(std::move(a), std::move(data_));
    completed_ = true;
}

CPPWAMP_INLINE void AuthorizationRequest::allow() {authorize(true);}

CPPWAMP_INLINE void AuthorizationRequest::deny() {authorize(false);}

CPPWAMP_INLINE void AuthorizationRequest::fail(std::error_code ec)
{
    CPPWAMP_LOGIC_CHECK(!completed_,
                        "wamp::AuthorizationRequest already completed");
    authorize(Authorization{ec});
    completed_ = true;
}

} // namespace wamp
