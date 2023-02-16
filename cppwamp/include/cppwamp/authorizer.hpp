/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_AUTHORIZER_HPP
#define CPPWAMP_AUTHORIZER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for dynamic authorization. */
//------------------------------------------------------------------------------

// TODO: Authorization cache

#include <cassert>
#include <functional>
#include <utility>
#include "any.hpp"
#include "anyhandler.hpp"
#include "api.hpp"
#include "authinfo.hpp"
#include "error.hpp"
#include "peerdata.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Determines how callers and publishers are disclosed. */
//------------------------------------------------------------------------------
enum class DisclosureRule
{
    preset,       ///< Reveal originator as per the realm configuration preset.
    originator,   ///< Reveal originator as per its `disclose_me` option.
    reveal,       ///< Reveal originator even if disclosure was not requested.
    conceal,      ///< Conceal originator even if disclosure was requested.
    strictReveal, ///< Reveal originator and disallow `disclose_me` option.
    strictConceal ///< Conceal originator and disallow `disclose_me` option.
};

//------------------------------------------------------------------------------
/** Contains authorization information on a operation. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Authorization
{
    /** Constructor taking a boolean indicating if the operation is allowed. */
    Authorization(bool allowed = true);

    /** Constructor taking an error code indicating that the authorization
        operation itself has failed. */
    Authorization(std::error_code ec);

    /** Sets the trust level for a publish or call operation, to be propagated
        to the corresponding events or invocation. */
    Authorization& withTrustLevel(TrustLevel tl);

    /** Sets the rule that governs how the caller/publisher is disclosed. */
    Authorization& withDisclosure(DisclosureRule d);

    /** Obtains the error code indicating if the authorization operation itself
        has failed. */
    std::error_code error() const;

    /** Determines if the operation is allowed. */
    bool allowed() const;

    /** Determines if the operation has a trust level. */
    bool hasTrustLevel() const;

    /** Obtains the operation's trust level. */
    TrustLevel trustLevel() const;

    /** Obtains the caller/publisher disclosure rule. */
    DisclosureRule disclosure() const;

private:
    std::error_code error_;
    TrustLevel trustLevel_ = 0;
    DisclosureRule disclosure_ = DisclosureRule::preset;
    bool allowed_ = false;
    bool hasTrustLevel_ = false;
};

//------------------------------------------------------------------------------
/** Indicates which operation is being authorized. */
//------------------------------------------------------------------------------
enum class AuthorizationAction
{
    publish,
    subscribe,
    enroll,
    call
};

//------------------------------------------------------------------------------
/** Contains information on an operation that is requesting authorization. */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthorizationRequest
{
public:
    /** Determines which opereration needs authorizing. */
    AuthorizationAction action() const;

    /** Accesses the authentication information of the originator. */
    const AuthInfo& authInfo() const;

    /** Accesses the data associated with a publish operation. */
    const Pub& pub() const;

    /** Accesses the data associated with a subscribe operation. */
    const Topic& topic() const;

    /** Accesses the data associated with a enroll (register) operation. */
    const Procedure& procedure() const;

    /** Accesses the data associated with an RPC call operation. */
    const Rpc& rpc() const;

    /** Accesses the data associated with the operation. */
    template <typename T>
    const T& dataAs() const;

    /** Applies the given visitor according to the authorization action. */
    template <typename TVisitor>
    void apply(TVisitor&& v) const;

    /** Authorizes the operation with the given authorization information. */
    void authorize(Authorization a);

    /** Allows the operation without proving any authorization information. */
    void allow();

    /** Denies the operation without proving any authorization information. */
    void deny();

    /** Marks the authorization operation itself as having failed. */
    void fail(std::error_code ec);

private:
    using Handler = std::function<void (Authorization, any)>;

    any data_;
    Handler handler_;
    AuthInfo::Ptr authInfo_;
    AuthorizationAction action_;
    bool completed_ = false;

public:
    // Internal use only
    template <typename TPeerData>
    AuthorizationRequest(internal::PassKey, AuthorizationAction a,
                         TPeerData&& d, AuthInfo::Ptr i, Handler h);
};

//------------------------------------------------------------------------------
using Authorizer = AnyReusableHandler<void (AuthorizationRequest)>;


//******************************************************************************
// AuthorizationRequest template member definitions
//******************************************************************************

/** @tparam T Either Pub, Topic, Procedure, or Rpc. */
template <typename T>
const T& AuthorizationRequest::dataAs() const
{
    auto data = any_cast<T>(&data_);
    CPPWAMP_LOGIC_CHECK(data != nullptr,
                        "wamp::AuthorizationRequest does not hold a T");
    return *data;
}

/** @tparam TVisitor A callable entity taking an AuthorizationRequest as its
                     first parameter, and either Pub, Topic, Procedure, or Rpc
                     its second parameter. */
template <typename TVisitor>
void AuthorizationRequest::apply(TVisitor&& v) const
{
    using V = TVisitor;
    using AA = AuthorizationAction;
    switch (action_)
    {
    case AA::publish:   std::forward<V>(v)(*this, pub());       break;
    case AA::subscribe: std::forward<V>(v)(*this, topic());     break;
    case AA::enroll:    std::forward<V>(v)(*this, procedure()); break;
    case AA::call:      std::forward<V>(v)(*this, rpc());       break;
    default: assert(false && "Unexpected AuthorizationAction enumerator");
    }
}

template <typename TPeerData>
AuthorizationRequest::AuthorizationRequest(
    internal::PassKey, AuthorizationAction a, TPeerData&& d, AuthInfo::Ptr i,
    Handler h)
    : data_(std::forward<TPeerData>(d)),
      handler_(std::move(h)),
      authInfo_(std::move(i)),
      action_(a)
{}

} // namespace wamp

#endif // CPPWAMP_AUTHORIZER_HPP
