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

#include <memory>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "disclosure.hpp"
#include "errorcodes.hpp"
#include "pubsubinfo.hpp"
#include "realmobserver.hpp"
#include "rpcinfo.hpp"
#include "sessioninfo.hpp"
#include "internal/authorizationlistener.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

namespace internal
{
class RouterSession;
}

//------------------------------------------------------------------------------
/** Enumerates the possible outcomes of an authorization. */
//------------------------------------------------------------------------------
enum class AuthorizationDecision
{
    granted, ///< Permission to complete the operation is granted.
    denied,  ///< Permission to complete the operation is denied.
    failed   ///< The authorization operation itself failed.
};


//------------------------------------------------------------------------------
/** Contains authorization information on a operation. */
//------------------------------------------------------------------------------
class CPPWAMP_API Authorization
{
public:
    /// Enumerates the possible outcomes of an authorization
    using Decision = AuthorizationDecision;

    // NOLINTBEGIN(google-explicit-constructor)

    /** Default constructs an instance indicating the authorization
        is granted. */
    Authorization();

    /** Constructs an instance indicating the authorization is granted. */
    static Authorization granted(Disclosure d = Disclosure::preset);

    /** Constructs an instance indicating the authorization is denied, along
        with additional error information to be returned to the originator. */
    static Authorization denied(std::error_code ec);

    /** Constructs an instance indicating the authorization is denied, along
        with optional additional error information to be returned to the
        originator. */
    static Authorization denied(WampErrc errc = WampErrc::authorizationDenied);

    /** Constructs an instance indicating the authorization is denied, along
        with additional error information to be returned to the originator. */
    static Authorization failed(std::error_code ec);

    /** Constructs an instance indicating the authorization is denied, along
        with optional additional error information to be returned to the
        originator. */
    static Authorization failed(WampErrc errc = WampErrc::authorizationFailed);

    // NOLINTEND(google-explicit-constructor)

    /** Returns true if and only if the authorization decision is
        Authorization::granted. */
    bool good() const;

    /** Obtains the authorization decision. */
    Decision decision() const;

    /** Obtains the caller/publisher disclosure mode. */
    Disclosure disclosure() const;

    /** Obtains the error code indicating indicating the reason for
        authorization denial or failure. */
    std::error_code error() const;

private:
    explicit Authorization(Decision decision, Disclosure disclosure,
                           std::error_code ec);

    std::error_code errorCode_;
    Decision decision_;
    Disclosure disclosure_;
};

class Authorizer;

//------------------------------------------------------------------------------
/** Contains information on an operation that is requesting authorization. */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthorizationRequest
{
public:
    /** Default constructor. */
    AuthorizationRequest();

    /** Accesses information on the originator. */
    const SessionInfo& info() const;

    /** Authorizes a subscribe operation. */
    void authorize(Topic t, Authorization a, bool cache = false);

    /** Authorizes a publish operation. */
    void authorize(Pub p, Authorization a, bool cache = false);

    /** Authorizes a register operation. */
    void authorize(Procedure p, Authorization a, bool cache = false);

    /** Authorizes a call operation. */
    void authorize(Rpc r, Authorization a, bool cache = false);

private:
    using Listener = internal::AuthorizationListener;
    using ListenerPtr = internal::AuthorizationListener::WeakPtr;
    using Originator = internal::RouterSession;

    template <typename C>
    void doAuthorize(C&& command, Authorization auth, bool cache);

    template <typename C>
    void grantAuthorization(C&& command, Authorization auth,
                            const std::shared_ptr<Originator>& originator,
                            internal::AuthorizationListener& listener);

    template <typename C>
    void rejectAuthorization(C&& command, Authorization auth, WampErrc errc,
                             internal::RouterSession& originator);

    Listener::WeakPtr listener_;
    std::weak_ptr<Originator> originator_;
    std::weak_ptr<Authorizer> authorizer_;
    SessionInfo info_;
    Disclosure realmDisclosure_;
    bool consumerDisclosure_;

public: // Internal use only
    AuthorizationRequest(internal::PassKey,
        ListenerPtr listener,
        const std::shared_ptr<Originator>& originator,
        const std::shared_ptr<Authorizer>& authorizer,
        Disclosure realmDisclosure, bool consumerDisclosure = false);
};

//------------------------------------------------------------------------------
/** Interface for user-defined authorizers. */
//------------------------------------------------------------------------------
class CPPWAMP_API Authorizer : public std::enable_shared_from_this<Authorizer>
{
public:
    /// Shared pointer type
    using Ptr = std::shared_ptr<Authorizer>;

    /** Destructor. */
    virtual ~Authorizer() = default;

    /** Authorizes a subscribe request. */
    virtual void authorize(Topic t, AuthorizationRequest a);

    /** Authorizes a publish request. */
    virtual void authorize(Pub p, AuthorizationRequest a);

    /** Authorizes a registration request. */
    virtual void authorize(Procedure p, AuthorizationRequest a);

    /** Authorizes a call request. */
    virtual void authorize(Rpc r, AuthorizationRequest a);

    /** Caches a subscribe authorization. */
    virtual void cache(const Topic& t, const SessionInfo& s, Authorization a);

    /** Caches a publish authorization. */
    virtual void cache(const Pub& p, const SessionInfo& s, Authorization a);

    /** Caches a register authorization. */
    virtual void cache(const Procedure& p, const SessionInfo& s,
                       Authorization a);

    /** Caches a call authorization. */
    virtual void cache(const Rpc& r, const SessionInfo& s, Authorization a);

    /** Called when a session leaves or is kicked from the realm. */
    virtual void uncacheSession(const SessionInfo&);

    /** Called when an RPC registration is removed. */
    virtual void uncacheProcedure(const RegistrationInfo&);

    /** Called when a subscription is removed. */
    virtual void uncacheTopic(const SubscriptionInfo&);

    /** Called by the router implementation to set the IO executor via
        which operations can be dispatched/posted. */
    virtual void setIoExecutor(const AnyIoExecutor& exec);

protected:
    /** Constructor taking an optional chained authorizer. */
    explicit Authorizer(Ptr chained = nullptr);

    /** Binds the given chained authorizer. */
    void bind(Ptr chained);

    /** Obtains the optional chained authorizer. */
    const Ptr& chained() const;

private:
    Ptr chained_;
};

//------------------------------------------------------------------------------
/** Posts authorization operations via en executor. */
//------------------------------------------------------------------------------
class CPPWAMP_API PostingAuthorizer : public Authorizer
{
public:
    /// Executor via which to post the authorize operations.
    using Executor = AnyCompletionExecutor;

    /// Shared pointer type.
    using Ptr = std::shared_ptr<PostingAuthorizer>;

    /** Creates an PostingAuthorizer instance. */
    static Ptr create(Authorizer::Ptr chained, Executor e);

    /** Obtains the executor via which authorize operations are to be posted */
    const Executor& executor() const;

    void authorize(Topic t, AuthorizationRequest a) override;

    void authorize(Pub p, AuthorizationRequest a) override;

    void authorize(Procedure p, AuthorizationRequest a) override;

    void authorize(Rpc r, AuthorizationRequest a) override;

private:
    using Base = Authorizer;

    explicit PostingAuthorizer(Authorizer::Ptr chained,
                               AnyCompletionExecutor e);

    template <typename C>
    void postAuthorization(C& command, AuthorizationRequest& req);

    void setIoExecutor(const AnyIoExecutor& exec) final;

    AnyCompletionExecutor executor_;
    AnyIoExecutor ioExecutor_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authorizer.inl.hpp"
#endif

#endif // CPPWAMP_AUTHORIZER_HPP
