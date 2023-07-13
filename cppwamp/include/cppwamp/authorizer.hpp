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

// TODO: Provide authorizer which blocks WAMP meta API
// https://github.com/wamp-proto/wamp-proto/discussions/489

#include <array>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include "anyhandler.hpp"
#include "api.hpp"
#include "asiodefs.hpp"
#include "disclosurerule.hpp"
#include "pubsubinfo.hpp"
#include "realmobserver.hpp"
#include "rpcinfo.hpp"
#include "sessioninfo.hpp"
#include "utils/triemap.hpp"
#include "internal/authorizationlistener.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

namespace internal
{
class RouterSession;
}

//------------------------------------------------------------------------------
/** Contains authorization information on a operation. */
//------------------------------------------------------------------------------
class CPPWAMP_API Authorization
{
public:
    // NOLINTBEGIN(google-explicit-constructor)

    /** Converting constructor taking a boolean indicating if the operation
        is allowed. */
    Authorization(bool allowed = true);

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

//------------------------------------------------------------------------------
/** Contains information on an operation that is requesting authorization. */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthorizationRequest
{
public:
    /** Accesses information on the originator. */
    const SessionInfo& info() const;

    /** Authorizes a subscribe operation. */
    void authorize(Topic t, Authorization a);

    /** Authorizes a publish operation. */
    void authorize(Pub p, Authorization a);

    /** Authorizes a register operation. */
    void authorize(Procedure p, Authorization a);

    /** Authorizes a call operation. */
    void authorize(Rpc r, Authorization a);

private:
    using ListenerPtr = internal::AuthorizationListener::WeakPtr;
    using Originator = internal::RouterSession;

    template <typename C>
    void doAuthorize(C&& command, Authorization auth);

    ListenerPtr listener_;
    std::weak_ptr<internal::RouterSession> originator_;
    SessionInfo info_;
    DisclosureRule realmDisclosure_ = DisclosureRule::preset;

public: // Internal use only
    AuthorizationRequest(
        internal::PassKey,
        ListenerPtr listener,
        const std::shared_ptr<internal::RouterSession>& originator,
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

    /** Binds an executor via which to post an authorization handler. */
    void bindExecutor(AnyCompletionExecutor e);

    /** Authorizes a subscribe request. */
    virtual void authorize(Topic t, AuthorizationRequest a, AnyIoExecutor& e);

    /** Authorizes a publish request. */
    virtual void authorize(Pub p, AuthorizationRequest a, AnyIoExecutor& e);

    /** Authorizes a registration request. */
    virtual void authorize(Procedure p, AuthorizationRequest a,
                           AnyIoExecutor& e);

    /** Authorizes a call request. */
    virtual void authorize(Rpc r, AuthorizationRequest a, AnyIoExecutor& e);

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
    Authorizer();

    /** Can be overridden to conditionally authorize a subscribe request. */
    virtual void onAuthorize(Topic t, AuthorizationRequest a);

    /** Can be overridden to conditionally authorize a publish request. */
    virtual void onAuthorize(Pub p, AuthorizationRequest a);

    /** Can be overridden to conditionally authorize a registration request. */
    virtual void onAuthorize(Procedure p, AuthorizationRequest a);

    /** Can be overridden to conditionally authorize a call request. */
    virtual void onAuthorize(Rpc r, AuthorizationRequest a);

    /** Accesses the bound executor. */
    AnyCompletionExecutor& executor();

private:
    template <typename C>
    void doAuthorize(C&& command, AuthorizationRequest&& a,
                     AnyIoExecutor& ioExec);

    AnyCompletionExecutor executor_;
};

//------------------------------------------------------------------------------
/** Customizable caching authorizer. */
//------------------------------------------------------------------------------
class CPPWAMP_API CachingAuthorizer : public Authorizer
{
public:
    using Predicate = std::function<bool (SessionInfo)>;

    void clearAll();

    void clearBySessionId(SessionId sid);

    void clearByAuthId(const String& authId);

    void clearByAuthRole(const String& authRole);

    void clearIf(const Predicate& pred);

protected:
    CachingAuthorizer();

    void authorize(Topic t, AuthorizationRequest a, AnyIoExecutor& e) override;

    void authorize(Pub p, AuthorizationRequest a, AnyIoExecutor& e) override;

    void authorize(Procedure p, AuthorizationRequest a,
                   AnyIoExecutor& e) override;

    void authorize(Rpc r, AuthorizationRequest a, AnyIoExecutor& e) override;

    using Authorizer::onAuthorize;

private:
    struct Record
    {
        SessionInfo info;
        Authorization auth;
    };

    using Base = Authorizer;
    using RecordsBySessionId = std::map<SessionId, Record>;
    using CacheByUri = utils::TrieMap<RecordsBySessionId>;
    using MutexGuard = std::lock_guard<std::mutex>;

    template <typename P>
    static void eraseFromCacheIf(CacheByUri& cache, const P& pred);

    static void eraseSessionFromCache(CacheByUri& cache, SessionId sid);

    CacheByUri& subscribeCacheForPolicy(MatchPolicy p);

    const CacheByUri& subscribeCacheForPolicy(MatchPolicy p) const;

    template <typename C>
    void doAuthorize(const CacheByUri& cache, C& command,
                     AuthorizationRequest& req, AnyIoExecutor& exec);

    template <typename P>
    void doClearIf(const P& pred);

    void cache(const Topic& t, const SessionInfo& s, Authorization a) final;

    void cache(const Pub& p, const SessionInfo& s, Authorization a) final;

    void cache(const Procedure& p, const SessionInfo& s, Authorization a) final;

    void cache(const Rpc& r, const SessionInfo& s, Authorization a) final;

    void uncacheSession(const SessionInfo& info) final;

    void uncacheTopic(const SubscriptionInfo& info) final;

    void uncacheProcedure(const RegistrationInfo& info) final;

    std::mutex mutex_;
    std::array<CacheByUri, 3> subscribeCaches_;
    CacheByUri publishCache_;
    CacheByUri registerCache_;
    CacheByUri callCache_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authorizer.inl.hpp"
#endif

#endif // CPPWAMP_AUTHORIZER_HPP
