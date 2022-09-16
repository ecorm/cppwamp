/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERIMPL_HPP
#define CPPWAMP_INTERNAL_ROUTERIMPL_HPP

#include <map>
#include <memory>
#include <thread>
#include <string>
#include <utility>
#include "../routerconfig.hpp"
#include "idgen.hpp"
#include "routercontext.hpp"
#include "routerrealm.hpp"
#include "routerserver.hpp"

namespace wamp
{


namespace internal
{

class LocalSessionImpl;

//------------------------------------------------------------------------------
class RouterImpl : public std::enable_shared_from_this<RouterImpl>
{
public:
    using Ptr = std::shared_ptr<RouterImpl>;
    using Executor = AnyIoExecutor;
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    static Ptr create(Executor exec, RouterConfig config)
    {
        return Ptr(new RouterImpl(std::move(exec), std::move(config)));
    }

    Server::Ptr addServer(ServerConfig config)
    {
        return nullptr;
    }

    void removeServer(const std::string& name)
    {

    }

    LocalSessionImpl::Ptr joinLocal(AuthorizationInfo a,
                                    AnyCompletionExecutor e)
    {
        auto realm = findOrCreateRealm(a.realmUri());
        a.setSessionId(sessionIdPool_->allocate());
        using std::move;
        auto session = LocalSessionImpl::create({realm}, move(a), move(e));
        realm->join(session);
        return session;
    }

    void startAll()
    {
        for (auto& kv: servers_)
            kv.second->start();
    }

    void stopAll()
    {
        for (auto& kv: servers_)
            kv.second->stop();
    }

    static const Object& roles()
    {
        static const Object routerRoles;
        return routerRoles;
    }

    const IoStrand& strand() const {return strand_;}

    Server::Ptr server(const std::string& name) const
    {
        auto found = servers_.find(name);
        if (found == servers_.end())
            return nullptr;
        return found->second;
    }

private:
    RouterImpl(Executor e, RouterConfig c)
        : strand_(boost::asio::make_strand(e)),
          logger_(RouterLogger::create(strand_, c.logHandler(), c.logLevel())),
          config_(std::move(c))
    {}

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    RouterLogger::Ptr logger() const {return logger_;}

    RealmContext joinRemote(ServerSession::Ptr s)
    {
        auto realm = findOrCreateRealm(s->authInfo().realmUri());
        realm->join(s);
        return {realm};
    }

    RouterRealm::Ptr findOrCreateRealm(const std::string& uri)
    {
        auto kv = realms_.find(uri);
        if (kv != realms_.end())
            return kv->second;

        auto realm = RouterRealm::create(strand_, {shared_from_this()}, uri);
        realms_.emplace(uri, realm);
        return realm;
    }

    IoStrand strand_;
    std::map<std::string, Server::Ptr> servers_;
    std::map<std::string, RouterRealm::Ptr> realms_;
    RandomIdPool::Ptr sessionIdPool_;
    RouterLogger::Ptr logger_;
    RouterConfig config_;

    friend class RouterContext;
};


//******************************************************************************
// RouterContext
//******************************************************************************

inline RouterContext::RouterContext(std::shared_ptr<RouterImpl> r)
    : router_(std::move(r))
{}

inline RouterLogger::Ptr RouterContext::logger() const
{
    auto r = router_.lock();
    if (r)
        return r->logger();
    return nullptr;
}

inline RealmContext RouterContext::join(std::shared_ptr<ServerSession> s)
{
    auto r = router_.lock();
    if (r)
        return r->joinRemote(std::move(s));
    return {};
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERIMPL_HPP
