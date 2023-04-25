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
#include <memory>
#include <utility>
#include "api.hpp"
#include "authinfo.hpp"
#include "pubsubinfo.hpp"
#include "rpcinfo.hpp"
#include "tagtypes.hpp"
#include "internal/passkey.hpp"
#include "internal/routercontext.hpp"

namespace wamp
{

namespace internal
{
class RouterSession;
}

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
class CPPWAMP_API Authorization
{
public:
    /** Constructor taking a boolean indicating if the operation is allowed. */
    Authorization(bool allowed = true);

    /** Constructor taking an error code indicating that the authorization
        operation itself has failed. */
    Authorization(std::error_code ec);

    /** Sets the rule that governs how the caller/publisher is disclosed. */
    Authorization& withDisclosure(DisclosureRule d);

    /** Returns true if the authorization succeeded and the operations
        is allowed. */
    explicit operator bool() const;

    /** Obtains the error code indicating if the authorization operation itself
        has failed. */
    std::error_code error() const;

    /** Determines if the operation is allowed. */
    bool allowed() const;

    /** Obtains the caller/publisher disclosure rule. */
    DisclosureRule disclosure() const;

private:
    std::error_code error_;
    DisclosureRule disclosure_ = DisclosureRule::preset;
    bool allowed_ = false;
};

//------------------------------------------------------------------------------
/** Contains information on an operation that is requesting authorization. */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthorizationRequest
{
public:
    /** Accesses the authentication information of the originator. */
    const AuthInfo& authInfo() const;

    /** Authorizes a subscribe operation. */
    void authorize(Topic t, Authorization a);

    /** Thread-safe authorized subscribe. */
    void authorize(ThreadSafe, Topic t, Authorization a);

    /** Authorizes a publish operation. */
    void authorize(Pub p, Authorization a);

    /** Thread-safe authorized publish. */
    void authorize(ThreadSafe, Pub p, Authorization a);

    /** Authorizes a register operation. */
    void authorize(Procedure p, Authorization a);

    /** Thread-safe authorized register. */
    void authorize(ThreadSafe, Procedure p, Authorization a);

    /** Authorizes a call operation. */
    void authorize(Rpc r, Authorization a);

    /** Thread-safe authorized call operation. */
    void authorize(ThreadSafe, Rpc r, Authorization a);

private:
    template <typename C>
    void send(C&& command, Authorization a);

    template <typename C>
    void send(ThreadSafe, C&& command, Authorization a);

    internal::RealmContext realm_;
    std::weak_ptr<internal::RouterSession> originator_;
    AuthInfo::Ptr authInfo_;
    DisclosureRule presetDisclosure_;
    bool completed_ = false;

public:
    // Internal use only
    AuthorizationRequest(internal::PassKey, internal::RealmContext r,
                         std::shared_ptr<internal::RouterSession> s);
};

//------------------------------------------------------------------------------
/** Interface for user-defined authorizers. */
//------------------------------------------------------------------------------
class CPPWAMP_API Authorizer
{
public:
    using Ptr = std::shared_ptr<Authorizer>;

    /** Destructor. */
    virtual ~Authorizer();

    /** Authorizes a subscribe request. */
    virtual void authorize(Topic t, AuthorizationRequest a);

    /** Authorizes a publish request. */
    virtual void authorize(Pub p, AuthorizationRequest a);

    /** Authorizes a registration request. */
    virtual void authorize(Procedure p, AuthorizationRequest a);

    /** Authorizes a call request. */
    virtual void authorize(Rpc r, AuthorizationRequest a);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authorizer.ipp"
#endif

#endif // CPPWAMP_AUTHORIZER_HPP
