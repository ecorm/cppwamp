/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../cachingauthorizer.hpp"
#include <cassert>
#include "../api.hpp"
#include "hashcombine.hpp"

namespace wamp
{

//******************************************************************************
// CachingAuthorizer::CacheKey
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::CacheKey::CacheKey(const Topic& subscribe)
    : uri(subscribe.uri()),
      policy(subscribe.matchPolicy()),
      action(Action::subscribe)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::CacheKey::CacheKey(const Pub& publish)
    : uri(publish.uri()),
      action(Action::publish)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::CacheKey::CacheKey(const Procedure& enroll)
    : uri(enroll.uri()),
      policy(enroll.matchPolicy()),
      action(Action::enroll)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::CacheKey::CacheKey(const Rpc& call)
    : uri(call.uri()),
      action(Action::call)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool
CachingAuthorizer::CacheKey::operator==(const CacheKey& key) const
{
    return (uri == key.uri) && (policy == key.policy) &&
           (action == key.action);
}

//******************************************************************************
// CachingAuthorizer::CacheKeyHash
//******************************************************************************

CPPWAMP_INLINE std::size_t
CachingAuthorizer::CacheKeyHash::operator()(const CacheKey& key) const
{
    std::size_t hash = std::hash<Uri>{}(key.uri);
    const auto p = static_cast<unsigned>(key.policy);
    const auto a = static_cast<unsigned>(key.action);
    const unsigned extra = (p << 8) | a;
    internal::hashCombine(hash, extra);
    return hash;
}


//******************************************************************************
// CachingAuthorizer
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::Ptr
CachingAuthorizer::create(Authorizer::Ptr chained, Size capacity)
{
    return Ptr{new CachingAuthorizer(std::move(chained), capacity)};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool CachingAuthorizer::empty() const {return cache_.empty();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::Size CachingAuthorizer::size() const
{
    return cache_.size();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::Size CachingAuthorizer::capacity() const
{
    return cache_.capacity();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE float CachingAuthorizer::loadFactor() const
{
    return cache_.loadFactor();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE float CachingAuthorizer::maxLoadFactor() const
{
    return cache_.maxLoadFactor();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::setMaxLoadFactor(float mlf)
{
    cache_.setMaxLoadFactor(mlf);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::clear()
{
    const MutexGuard guard{mutex_};
    cache_.clear();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::evictBySessionId(SessionId sid)
{
    const MutexGuard guard{mutex_};
    cache_.evictIf(
        [sid](const CacheKey&, const CacheEntry& entry) -> bool
        {
            return entry.info.sessionId() == sid;
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::evictByAuthId(const String& authId)
{
    const MutexGuard guard{mutex_};
    cache_.evictIf(
        [&authId](const CacheKey&, const CacheEntry& entry) -> bool
        {
            return entry.info.auth().id() == authId;
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::evictByAuthRole(const String& authRole)
{
    const MutexGuard guard{mutex_};
    cache_.evictIf(
        [&authRole](const CacheKey&, const CacheEntry& entry) -> bool
        {
            return entry.info.auth().role() == authRole;
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::evictIf(const Predicate& pred)
{
    const MutexGuard guard{mutex_};
    cache_.evictIf(
        [&pred](const CacheKey&, const CacheEntry& entry) -> bool
        {
            return pred(entry.info);
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(Topic t,
                                                 AuthorizationRequest a)
{
    cachedAuthorize(t, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(Pub p, AuthorizationRequest a)
{
    cachedAuthorize(p, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(Procedure p,
                                                 AuthorizationRequest a)
{
    cachedAuthorize(p, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(Rpc r, AuthorizationRequest a)
{
    cachedAuthorize(r, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::CachingAuthorizer(Authorizer::Ptr chained,
                                                    std::size_t capacity)
    : Base(std::move(chained)),
      cache_(capacity)
{}

//------------------------------------------------------------------------------
template <typename C>
void CachingAuthorizer::cachedAuthorize(C& command, AuthorizationRequest& req)
{
    const Authorization* auth = nullptr;

    {
        const MutexGuard guard{mutex_};
        const CacheEntry* entry = cache_.lookup(CacheKey{command});
        if (entry != nullptr)
            auth = &(entry->auth);
    }

    if (auth == nullptr)
        Base::chained()->authorize(std::move(command), std::move(req));
    else
        req.authorize(std::move(command), *auth);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(
    const Topic& t, const SessionInfo& s, Authorization a)
{
    const MutexGuard guard{mutex_};
    cache_.upsert(CacheKey{t}, CacheEntry{s, a});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(const Pub& p, const SessionInfo& s,
                                             Authorization a)
{
    const MutexGuard guard{mutex_};
    cache_.upsert(CacheKey{p}, CacheEntry{s, a});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(
    const Procedure& p, const SessionInfo& s, Authorization a)
{
    const MutexGuard guard{mutex_};
    cache_.upsert(CacheKey{p}, CacheEntry{s, a});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(const Rpc& r, const SessionInfo& s,
                                             Authorization a)
{
    const MutexGuard guard{mutex_};
    cache_.upsert(CacheKey{r}, CacheEntry{s, a});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void
CachingAuthorizer::uncacheSession(const SessionInfo& info)
{
    evictBySessionId(info.sessionId());
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void
CachingAuthorizer::uncacheTopic(const SubscriptionInfo& info)
{
    const MutexGuard guard{mutex_};
    cache_.evictIf(
        [&info](const CacheKey& key, const CacheEntry&) -> bool
        {
            return key.action == Action::subscribe &&
                   key.policy == info.matchPolicy &&
                   key.uri == info.uri;
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void
CachingAuthorizer::uncacheProcedure(const RegistrationInfo& info)
{
    const MutexGuard guard{mutex_};
    cache_.evictIf(
        [&info](const CacheKey& key, const CacheEntry&) -> bool
        {
            return key.action == Action::call &&
                   key.uri == info.uri;
        });
}

} // namespace wamp
