/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CACHINGAUTHORIZER_HPP
#define CPPWAMP_CACHINGAUTHORIZER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for dynamic authorization. */
//------------------------------------------------------------------------------

#include <functional>
#include <mutex>
#include "authorizer.hpp"
#include "internal/lrucache.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Customizable caching authorizer. */
//------------------------------------------------------------------------------
class CPPWAMP_API CachingAuthorizer : public Authorizer
{
public:
    /// Shared pointer type.
    using Ptr = std::shared_ptr<CachingAuthorizer>;

    /// Preducate type used by CachingAuthorizer::evictIf.
    using Predicate = std::function<bool (SessionInfo)>;

    /// Cache size type.
    using Size = std::size_t;

    /** Creates a CachingAuthorizer instance. */
    static Ptr create(Authorizer::Ptr chained, Size capacity);

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

    void authorize(Topic t, AuthorizationRequest a) override;

    void authorize(Pub p, AuthorizationRequest a) override;

    void authorize(Procedure p, AuthorizationRequest a) override;

    void authorize(Rpc r, AuthorizationRequest a) override;

private:
    using Base = Authorizer;
    using MutexGuard = std::lock_guard<std::mutex>;

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

    CachingAuthorizer(Authorizer::Ptr chained, std::size_t capacity);

    template <typename C>
    void cachedAuthorize(C& command, AuthorizationRequest& req);

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
#include "internal/cachingauthorizer.inl.hpp"
#endif

#endif // CPPWAMP_CACHINGAUTHORIZER_HPP
