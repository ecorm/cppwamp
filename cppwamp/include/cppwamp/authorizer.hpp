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

// TODO: Provide authorizer which blocks WAMP meta API
// https://github.com/wamp-proto/wamp-proto/discussions/489

#include <memory>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "disclosurerule.hpp"
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
/** Type that can be implicitly converted to an Authorization, indicating
    that the operation is allowed.
    @see wamp::granted */
//------------------------------------------------------------------------------
struct AuthorizationGranted
{
    constexpr AuthorizationGranted() noexcept = default;
};

/** Convenient AuthorizationGranted instance that can be passed to a function
    expecting an Authorization. */
static constexpr AuthorizationGranted granted;


//------------------------------------------------------------------------------
/** Type that can be implicitly converted to an Authorization, indicating
    that the operation is rejected.
    @see wamp::denied */
//------------------------------------------------------------------------------
struct AuthorizationDenied
{
    constexpr AuthorizationDenied() noexcept = default;
};

/** Convenient AuthorizationGranted instance that can be passed to a function
    expecting an Authorization. */
static constexpr AuthorizationDenied denied;


//------------------------------------------------------------------------------
/** Contains authorization information on a operation. */
//------------------------------------------------------------------------------
class CPPWAMP_API Authorization
{
public:
    /** Constructor taking a boolean indicating if the operation
        is allowed. */
    explicit Authorization(bool allowed = true);

    // NOLINTBEGIN(google-explicit-constructor)

    /** Converting constructor taking an AuthorizationGranted tag type. */
    Authorization(AuthorizationGranted);

    /** Converting constructor taking an AuthorizationDenied tag type. */
    Authorization(AuthorizationDenied);

    /** Converting constructor taking an error code indicating that the
        authorization operation itself has failed. */
    Authorization(std::error_code ec);

    /** Converting constructor taking a WampErrc enumerator indicating that the
        authorization operation itself has failed. */
    Authorization(WampErrc errc);

    // NOLINTEND(google-explicit-constructor)

    /** Sets the rule that governs how the caller/publisher is disclosed. */
    Authorization& withDisclosure(DisclosureRule d);

    /** Returns true if the authorization succeeded and the operations
        is allowed. */
    bool good() const;

    /** Obtains the error code indicating if the authorization operation itself
        has failed. */
    std::error_code error() const;

    /** Determines if the operation is allowed. */
    bool allowed() const;

    /** Obtains the caller/publisher disclosure rule. */
    DisclosureRule disclosure() const;

private:
    std::error_code errorCode_;
    DisclosureRule disclosure_ = DisclosureRule::preset;
    bool allowed_ = false;
};

class Authorizer;

//------------------------------------------------------------------------------
/** Contains information on an operation that is requesting authorization. */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthorizationRequest
{
public:
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
    using ListenerPtr = internal::AuthorizationListener::WeakPtr;
    using Originator = internal::RouterSession;

    template <typename C>
    void doAuthorize(C&& command, Authorization auth, bool cache);

    ListenerPtr listener_;
    std::weak_ptr<internal::RouterSession> originator_;
    std::weak_ptr<Authorizer> authorizer_;
    SessionInfo info_;
    DisclosureRule realmDisclosure_ = DisclosureRule::preset;

public: // Internal use only
    AuthorizationRequest(internal::PassKey,
        ListenerPtr listener,
        const std::shared_ptr<internal::RouterSession>& originator,
        const std::shared_ptr<Authorizer>& authorizer,
        DisclosureRule realmDisclosure);
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

    /** Called by the router implementation to set the IO executor via
        which operations can be dispatched/posted. */
    void setIoExecutor(AnyIoExecutor exec);

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

protected:
    /** Constructor taking an optional chained authorizer. */
    explicit Authorizer(Ptr chained = nullptr);

    /** Binds the given chained authorizer. */
    void bind(Ptr chained);

    /** Obtains the IO executor via which operations can be
        dispatched/posted. */
    AnyIoExecutor& ioExecutor();

    /** Obtains the optional chained authorizer. */
    const Ptr& chained() const;

private:
    AnyIoExecutor ioExecutor_;
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

    AnyCompletionExecutor executor_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authorizer.inl.hpp"
#endif

#endif // CPPWAMP_AUTHORIZER_HPP
