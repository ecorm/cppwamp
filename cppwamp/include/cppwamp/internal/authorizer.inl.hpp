/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authorizer.hpp"
#include "../api.hpp"
#include "../traits.hpp"
#include "disclosuresetter.hpp"
#include "routersession.hpp"

namespace wamp
{

//******************************************************************************
// Authorization
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(bool allowed) : allowed_(allowed) {}

//------------------------------------------------------------------------------
/** If WampErrc::authorizationDenied, WampErrc::authorizationFailed, or
    WampErrc::discloseMeDisallowed is passed, their corresponding URI shall be
    used in the ERROR message returned to the client. Otherwise, the error
    URI shall be `wamp.error.authorization_failed` and the ERROR message will
    contain two positional arguments:
    - A string formatted as `<ec.category().name()>:<ec.value()`
    - A string containing `ec.message()` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(std::error_code ec) : errorCode_(ec) {}

//------------------------------------------------------------------------------
/** @copydetails Authorization(std::error_code) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(WampErrc errc)
    : Authorization(make_error_code(errc))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization& Authorization::withDisclosure(DisclosureRule d)
{
    disclosure_ = d;
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Authorization::good() const {return !errorCode_ && allowed_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::error_code Authorization::error() const {return errorCode_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Authorization::allowed() const {return allowed_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE DisclosureRule Authorization::disclosure() const
{
    return disclosure_;
}


//******************************************************************************
// AuthorizationRequest
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE const SessionInfo& AuthorizationRequest::info() const
{
    return info_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Topic t, Authorization a)
{
    doAuthorize(std::move(t), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Pub p, Authorization a)
{
    doAuthorize(std::move(p), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Procedure p,
                                                    Authorization a)
{
    doAuthorize(std::move(p), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Rpc r, Authorization a)
{
    doAuthorize(std::move(r), a);
}

//------------------------------------------------------------------------------
template <typename C>
void AuthorizationRequest::doAuthorize(C&& command, Authorization auth)
{
    auto originator = originator_.lock();
    if (!originator)
        return;
    auto listener = listener_.lock();
    if (!listener)
        return;

    if (auth.good())
    {
        if (internal::DisclosureSetter::applyToCommand(
                command, *originator, realmDisclosure_, auth.disclosure()))
        {
            listener->onAuthorized(originator, std::forward<C>(command));
        }
        return;
    }

    auto ec = make_error_code(WampErrc::authorizationDenied);
    auto authEc = auth.error();
    bool isKnownAuthError = true;

    if (authEc)
    {
        isKnownAuthError =
            authEc == WampErrc::authorizationDenied ||
            authEc == WampErrc::authorizationFailed ||
            authEc == WampErrc::authorizationRequired ||
            authEc == WampErrc::discloseMeDisallowed;

        ec = isKnownAuthError ?
                 authEc :
                 make_error_code(WampErrc::authorizationFailed);
    }

    auto error = Error::fromRequest({}, command, ec);
    if (!isKnownAuthError)
        error.withArgs(briefErrorCodeString(authEc), authEc.message());

    originator->sendRouterCommand(std::move(error), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AuthorizationRequest::AuthorizationRequest(
    internal::PassKey, ListenerPtr listener,
    const std::shared_ptr<internal::RouterSession>& originator,
    DisclosureRule realmDisclosure)
    : listener_(std::move(listener)),
      originator_(originator),
      info_(originator->sharedInfo()),
      realmDisclosure_(realmDisclosure)
{}


//******************************************************************************
// Authorizer
//******************************************************************************

//------------------------------------------------------------------------------
/** @details
    This method makes it so that the `onAuthorize` handler will be posted
    via the given executor. If no executor is set, the `onAuthorize` handler
    is invoked directly from the realm's execution strand. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::bindExecutor(AnyCompletionExecutor e)
{
    executor_ = std::move(e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Topic t, AuthorizationRequest a,
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(t), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Pub p, AuthorizationRequest a,
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(p), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Procedure p, AuthorizationRequest a,
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(p), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Rpc r, AuthorizationRequest a,
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(r), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Topic& t, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Pub& p, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Procedure& p, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Rpc& r, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::uncacheSession(const SessionInfo&) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::uncacheProcedure(const RegistrationInfo&) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::uncacheTopic(const SubscriptionInfo&) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorizer::Authorizer() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Topic t, AuthorizationRequest a)
{
    a.authorize(std::move(t), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Pub p, AuthorizationRequest a)
{
    a.authorize(std::move(p), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Procedure p, AuthorizationRequest a)
{
    a.authorize(std::move(p), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Rpc r, AuthorizationRequest a)
{
    a.authorize(std::move(r), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnyCompletionExecutor& Authorizer::executor() {return executor_;}

//------------------------------------------------------------------------------
template <typename C>
void Authorizer::doAuthorize(C&& command, AuthorizationRequest&& a,
                             AnyIoExecutor& ioExec)
{
    using Command = ValueTypeOf<C>;

    if (executor_ == nullptr)
    {
        onAuthorize(std::forward<C>(command), std::move(a));
        return;
    }

    struct Posted
    {
        Ptr self;
        Command c;
        AuthorizationRequest a;
        void operator()() {self->onAuthorize(std::move(c), std::move(a));}
    };

    boost::asio::post(
        ioExec,
        boost::asio::bind_executor(
            executor_,
            Posted{shared_from_this(), std::forward<C>(command),
                   std::move(a)}));
}


//******************************************************************************
// CachingAuthorizer
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::clearAll()
{
    const MutexGuard guard{mutex_};

    for (auto& cache: subscribeCaches_)
        cache.clear();
    publishCache_.clear();
    registerCache_.clear();
    callCache_.clear();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::clearBySessionId(SessionId sid)
{
    const MutexGuard guard{mutex_};

    for (auto& cache: subscribeCaches_)
        eraseSessionFromCache(cache, sid);
    eraseSessionFromCache(publishCache_, sid);
    eraseSessionFromCache(registerCache_, sid);
    eraseSessionFromCache(callCache_, sid);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::clearByAuthId(const String& authId)
{
    doClearIf(
        [&authId](const SessionInfo& info) -> bool
        {
            return info.auth().id() == authId;
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::clearByAuthRole(const String& authRole)
{
    doClearIf(
        [&authRole](const SessionInfo& info) -> bool
        {
            return info.auth().role() == authRole;
        });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::clearIf(const Predicate& pred)
{
    doClearIf(pred);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::CachingAuthorizer() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(
    Topic t, AuthorizationRequest a, AnyIoExecutor& e)
{
    const CacheByUri& cache = subscribeCacheForPolicy(t.matchPolicy());
    doAuthorize(cache, t, a, e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(Pub p, AuthorizationRequest a,
                                                 AnyIoExecutor& e)
{
    doAuthorize(publishCache_, p, a, e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(
    Procedure p, AuthorizationRequest a, AnyIoExecutor& e)
{
    doAuthorize(registerCache_, p, a, e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::authorize(Rpc r, AuthorizationRequest a,
                                                 AnyIoExecutor& e)
{
    doAuthorize(callCache_, r, a, e);
}

//------------------------------------------------------------------------------
template <typename P>
void CachingAuthorizer::eraseFromCacheIf(CacheByUri& cache, const P& pred)
{
    for (auto& recordMap: cache)
    {
        auto iter = recordMap.begin();
        auto end = recordMap.cend();
        while (iter != end)
        {
            if (pred(iter->second.info))
                iter = recordMap.erase(iter);
            else
                ++iter;
        }
    }
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::eraseSessionFromCache(CacheByUri& cache,
                                                             SessionId sid)
{
    for (auto& recordMap: cache)
        recordMap.erase(sid);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CachingAuthorizer::CacheByUri&
CachingAuthorizer::subscribeCacheForPolicy(MatchPolicy p)
{
    assert(p != MatchPolicy::unknown);
    const unsigned index = static_cast<unsigned>(p) -
                           static_cast<unsigned>(MatchPolicy::exact);
    return subscribeCaches_.at(index);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const CachingAuthorizer::CacheByUri&
CachingAuthorizer::subscribeCacheForPolicy(MatchPolicy p) const
{
    assert(p != MatchPolicy::unknown);
    const unsigned index = static_cast<unsigned>(p) -
                           static_cast<unsigned>(MatchPolicy::exact);
    return subscribeCaches_.at(index);
}

//------------------------------------------------------------------------------
template <typename C>
void CachingAuthorizer::doAuthorize(const CacheByUri& cache, C& command,
                                    AuthorizationRequest& req,
                                    AnyIoExecutor& exec)
{
    const Authorization* auth = nullptr;

    {
        const MutexGuard guard{mutex_};

        const auto kv = cache.find(command.uri());
        if (kv != cache.end())
        {
            const RecordsBySessionId& authMap = kv.value();
            const auto iter = authMap.find(req.info().sessionId());
            if (iter != authMap.end())
                auth = &(iter->second.auth);
        }
    }

    if (auth == nullptr)
        Base::authorize(std::move(command), std::move(req), exec);
    else
        req.authorize(std::move(command), auth);
}

//------------------------------------------------------------------------------
template <typename P>
void CachingAuthorizer::doClearIf(const P& pred)
{
    const MutexGuard guard{mutex_};

    for (auto& cache: subscribeCaches_)
        eraseFromCacheIf(cache, pred);
    eraseFromCacheIf(publishCache_, pred);
    eraseFromCacheIf(registerCache_, pred);
    eraseFromCacheIf(callCache_, pred);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(
    const Topic& t, const SessionInfo& s, Authorization a)
{
    const MutexGuard guard{mutex_};
    CacheByUri& cache = subscribeCacheForPolicy(t.matchPolicy());
    cache[t.uri()][s.sessionId()] = Record{s, a};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(const Pub& p, const SessionInfo& s,
                                             Authorization a)
{
    const MutexGuard guard{mutex_};
    publishCache_[p.uri()][s.sessionId()] = Record{s, a};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(
    const Procedure& p, const SessionInfo& s, Authorization a)
{
    const MutexGuard guard{mutex_};
    publishCache_[p.uri()][s.sessionId()] = Record{s, a};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void CachingAuthorizer::cache(const Rpc& r, const SessionInfo& s,
                                             Authorization a)
{
    const MutexGuard guard{mutex_};
    publishCache_[r.uri()][s.sessionId()] = Record{s, a};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void
CachingAuthorizer::uncacheSession(const SessionInfo& info)
{
    const MutexGuard guard{mutex_};

    const auto sid = info.sessionId();
    for (auto& cache: subscribeCaches_)
        eraseSessionFromCache(cache, sid);
    eraseSessionFromCache(publishCache_, sid);
    eraseSessionFromCache(registerCache_, sid);
    eraseSessionFromCache(callCache_, sid);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void
CachingAuthorizer::uncacheTopic(const SubscriptionInfo& info)
{
    const MutexGuard guard{mutex_};
    CacheByUri& cache = subscribeCacheForPolicy(info.matchPolicy);
    cache.erase(info.uri);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void
CachingAuthorizer::uncacheProcedure(const RegistrationInfo& info)
{
    const MutexGuard guard{mutex_};
    callCache_.erase(info.uri);
}

} // namespace wamp
