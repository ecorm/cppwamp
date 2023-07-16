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
#include "internal/lrucache.hpp"
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
    using Size = std::size_t;

    /** Determines if the cache is empty. */
    bool empty() const;

    /** Obtains the number of entries in the cache. */
    Size size() const;

    /** Obtains the maximum allowable number of entries in the cache. */
    Size capacity() const;

    /** Obtains the current number of elements per bucket of the
        underlying unordered map. */
    float loadFactor() const;

    /** Obtains the current maximum load factor of the underlying
        unordered map. */
    float maxLoadFactor() const;

    /** Sets the maximum load factor of the underlying unordered map. */
    void setMaxLoadFactor(float mlf);

    /** Clears all entries from the cache. */
    void clear();

    /** Removes all cache entries having the given session ID. */
    void evictBySessionId(SessionId sid);

    /** Removes all cache entries having the given authId. */
    void evictByAuthId(const String& authId);

    /** Removes all cache entries having the given authRole. */
    void evictByAuthRole(const String& authRole);

    /** Removes all cache entries meeting the criteria of the given
        predicate function. */
    void evictIf(const Predicate& pred);

protected:
    explicit CachingAuthorizer(std::size_t capacity);

    void authorize(Topic t, AuthorizationRequest a, AnyIoExecutor& e) override;

    void authorize(Pub p, AuthorizationRequest a, AnyIoExecutor& e) override;

    void authorize(Procedure p, AuthorizationRequest a,
                   AnyIoExecutor& e) override;

    void authorize(Rpc r, AuthorizationRequest a, AnyIoExecutor& e) override;

    using Authorizer::onAuthorize;

private:
    enum Action
    {
        subscribe,
        publish,
        enroll,
        call
    };

    class CacheKey
    {
    public:
        explicit CacheKey(const Topic& subscribe);
        explicit CacheKey(const Pub& publish);
        explicit CacheKey(const Procedure& enroll);
        explicit CacheKey(const Rpc& call);
        bool operator==(const CacheKey& key) const;

        Uri uri;
        MatchPolicy policy = MatchPolicy::unknown;
        Action action;
    };

    struct CacheKeyHash
    {
        std::size_t operator()(const CacheKey& key) const;
    };

    struct CacheEntry
    {
        SessionInfo info;
        Authorization auth;
    };

    using Base = Authorizer;
    using EntriesBySessionId = std::map<SessionId, CacheEntry>;
    using CacheByUri = utils::TrieMap<EntriesBySessionId>;
    using MutexGuard = std::lock_guard<std::mutex>;

    template <typename C>
    void doAuthorize(C& command, AuthorizationRequest& req,
                     AnyIoExecutor& exec);

    void cache(const Topic& t, const SessionInfo& s, Authorization a) final;

    void cache(const Pub& p, const SessionInfo& s, Authorization a) final;

    void cache(const Procedure& p, const SessionInfo& s, Authorization a) final;

    void cache(const Rpc& r, const SessionInfo& s, Authorization a) final;

    void uncacheSession(const SessionInfo& info) final;

    void uncacheTopic(const SubscriptionInfo& info) final;

    void uncacheProcedure(const RegistrationInfo& info) final;

    std::mutex mutex_;
    internal::LruCache<CacheKey, CacheEntry, CacheKeyHash> cache_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authorizer.inl.hpp"
#endif

#endif // CPPWAMP_AUTHORIZER_HPP
