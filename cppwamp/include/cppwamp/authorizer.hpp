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
    virtual void cache(const Topic& t, SessionId s, Authorization a);

    /** Caches a publish authorization. */
    virtual void cache(const Pub& p, SessionId s, Authorization a);

    /** Caches a register authorization. */
    virtual void cache(const Procedure& p, SessionId s, Authorization a);

    /** Caches a call authorization. */
    virtual void cache(const Rpc& r, SessionId s, Authorization a);

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
/** User-defined caching authorizer. */
//------------------------------------------------------------------------------
class CPPWAMP_API CachingAuthorizer : public Authorizer
{
protected:
    CachingAuthorizer() = default;

    void authorize(Topic t, AuthorizationRequest a, AnyIoExecutor& e) override
    {
        const Cache& cache = subscribeCacheForPolicy(t.matchPolicy());
        doAuthorize(cache, t, a, e);
    }

    void authorize(Pub p, AuthorizationRequest a, AnyIoExecutor& e) override
    {
        doAuthorize(publishCache_, p, a, e);
    }

    void authorize(Procedure p, AuthorizationRequest a,
                   AnyIoExecutor& e) override
    {
        doAuthorize(registerCache_, p, a, e);
    }

    void authorize(Rpc r, AuthorizationRequest a, AnyIoExecutor& e) override
    {
        doAuthorize(callCache_, r, a, e);
    }

    using Authorizer::onAuthorize;

private:
    using Base = Authorizer;
    using AuthMap = std::map<SessionId, Authorization>; // TODO: Flat map
    using Cache = utils::TrieMap<AuthMap>;

    Cache& subscribeCacheForPolicy(MatchPolicy p)
    {
        assert(p != MatchPolicy::unknown);
        const unsigned index = static_cast<unsigned>(p) -
                               static_cast<unsigned>(MatchPolicy::exact);
        return subscribeCaches_.at(index);
    }

    const Cache& subscribeCacheForPolicy(MatchPolicy p) const
    {
        assert(p != MatchPolicy::unknown);
        const unsigned index = static_cast<unsigned>(p) -
                               static_cast<unsigned>(MatchPolicy::exact);
        return subscribeCaches_.at(index);
    }

    template <typename C>
    void doAuthorize(const Cache& cache, C& command,
                     AuthorizationRequest& req, AnyIoExecutor& exec)
    {
        const auto kv = cache.find(command.uri());
        if (kv == cache.end())
            return Base::authorize(std::move(command), std::move(req), exec);

        const AuthMap& authMap = kv.value();
        const auto iter = authMap.find(req.info().sessionId());
        if (iter == authMap.end())
            return Base::authorize(std::move(command), std::move(req), exec);

        const Authorization& auth = iter->second;
        req.authorize(std::move(command), auth);
    }

    void cache(const Topic& t, SessionId s, Authorization a) final
    {
        Cache& cache = subscribeCacheForPolicy(t.matchPolicy());
        cache[t.uri()][s] = a;
    }

    void cache(const Pub& p, SessionId s, Authorization a) final
    {
        publishCache_[p.uri()][s] = a;
    }

    void cache(const Procedure& p, SessionId s, Authorization a) final
    {
        publishCache_[p.uri()][s] = a;
    }

    void cache(const Rpc& r, SessionId s, Authorization a) final
    {
        publishCache_[r.uri()][s] = a;
    }

    void uncacheSession(const SessionInfo& info) final
    {
        const auto sid = info.sessionId();
        for (auto& cache: subscribeCaches_)
            eraseSessionFromCache(cache, sid);
        eraseSessionFromCache(publishCache_, sid);
        eraseSessionFromCache(registerCache_, sid);
        eraseSessionFromCache(callCache_, sid);
    }

    static void eraseSessionFromCache(Cache& cache, SessionId sid)
    {
        for (auto& authMap: cache)
            authMap.erase(sid);
    }

    void uncacheTopic(const SubscriptionInfo& info) final
    {
        Cache& cache = subscribeCacheForPolicy(info.matchPolicy);
        cache.erase(info.uri);
    }

    void uncacheProcedure(const RegistrationInfo& info) final
    {
        callCache_.erase(info.uri);
    }

    std::array<Cache, 3> subscribeCaches_;
    Cache publishCache_;
    Cache registerCache_;
    Cache callCache_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authorizer.inl.hpp"
#endif

#endif // CPPWAMP_AUTHORIZER_HPP
