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
#include "../erroror.hpp"
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

    static Ptr create(Executor exec, RouterConfig config)
    {
        return Ptr(new RouterImpl(std::move(exec), std::move(config)));
    }

    bool addRealm(RealmConfig c)
    {
        auto uri = c.uri();
        if (realms_.find(uri) != realms_.end())
        {
            log({LogLevel::warning,
                 "Rejected attempt to add realm with duplicate URI '" + uri + "'"});
            return false;
        }

        log({LogLevel::info, "Adding realm '" + uri + "'"});
        using std::move;
        auto r = RouterRealm::create(strand_, move(c), {shared_from_this()});
        realms_.emplace(move(uri), move(r));
        return true;
    }

    bool shutDownRealm(const std::string& name)
    {
        auto kv = realms_.find(name);
        if (kv == realms_.end())
        {
            log({LogLevel::warning,
                 "Attempting to shut down non-existent realm named '" + name + "'"});
            return false;
        }

        kv->second->shutDown();
        realms_.erase(kv);
        return true;
    }

    bool terminateRealm(const std::string& name)
    {
        auto kv = realms_.find(name);
        if (kv == realms_.end())
        {
            log({LogLevel::warning,
                 "Attempting to terminate non-existent realm named '" + name + "'"});
            return false;
        }

        kv->second->terminate();
        realms_.erase(kv);
        return true;
    }

    bool startServer(ServerConfig c)
    {
        auto name = c.name();
        if (servers_.find(name) != servers_.end())
        {
            log({LogLevel::warning,
                 "Rejected attempt to start a server with duplicate name '" + name + "'"});
            return false;
        }

        using std::move;
        auto s = RouterServer::create(strand_, move(c), {shared_from_this()});
        servers_.emplace(std::move(name), s);
        s->start();
        return true;
    }

    bool shutDownServer(const std::string& name)
    {
        auto kv = servers_.find(name);
        if (kv == servers_.end())
        {
            log({LogLevel::warning,
                 "Attempting to shut down non-existent server named '" + name + "'"});
            return false;
        }

        kv->second->shutDown();
        servers_.erase(kv);
        return true;
    }

    bool terminateServer(const std::string& name)
    {
        auto kv = servers_.find(name);
        if (kv == servers_.end())
        {
            log({LogLevel::warning,
                 "Attempting to terminate non-existent server named '" + name + "'"});
            return false;
        }

        kv->second->terminate();
        servers_.erase(kv);
        return true;
    }

    LocalSessionImpl::Ptr joinLocal(AuthorizationInfo a,
                                    AnyCompletionExecutor e)
    {
        auto kv = realms_.find(a.realmUri());
        if (kv == realms_.end())
            return nullptr;

        auto realm = kv->second;
        a.setSessionId(allocateSessionId());
        using std::move;
        auto session = LocalSessionImpl::create({realm}, move(a), move(e));
        realm->join(session);
        return session;
    }

    void shutDown()
    {
        log({LogLevel::info, "Shutting down router"});
        for (auto& kv: servers_)
            kv.second->shutDown();
        servers_.clear();
        for (auto& kv: realms_)
            kv.second->kickLocalSessions();
        realms_.clear();
        // TODO: Wait for GOODBYE acknowledgements or timeout
    }

    void terminate()
    {
        log({LogLevel::info, "Terminating router"});
        for (auto& kv: servers_)
            kv.second->terminate();
        servers_.clear();
        for (auto& kv: realms_)
            kv.second->kickLocalSessions();
        realms_.clear();
    }

    const IoStrand& strand() const {return strand_;}

private:
    RouterImpl(Executor e, RouterConfig c)
        : strand_(boost::asio::make_strand(e)),
          sessionIdPool_(c.sessionIdSeed()),
          logger_(RouterLogger::create(strand_, c.logHandler(), c.logLevel())),
          config_(std::move(c))
    {}

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    void log(LogEntry&& e) {logger_->log(std::move(e));}

    RouterLogger::Ptr logger() const {return logger_;}

    SessionId allocateSessionId()
    {
        return sessionIdPool_.allocate();
    }

    ErrorOr<RealmContext> joinRemote(ServerSession::Ptr s)
    {
        auto kv = realms_.find(s->authInfo()->realmUri());
        if (kv == realms_.end())
            return makeUnexpectedError(SessionErrc::noSuchRealm);
        auto realm = kv->second;
        realm->join(s);
        return {realm};
    }

    IoStrand strand_;
    std::map<std::string, RouterServer::Ptr> servers_;
    std::map<std::string, RouterRealm::Ptr> realms_;
    RandomIdPool sessionIdPool_;
    RouterLogger::Ptr logger_;
    RouterConfig config_;

    friend class RouterContext;
};


//******************************************************************************
// RouterContext
//******************************************************************************

inline const Object& RouterContext::roles()
{
    static const Object routerRoles =
    {
        {"dealer", Object{{"features", Object{{
            {"call_canceling", true},
            {"caller_identification", true}
        }}}}},
        {"broker", Object{{"features", Object{{
            {"publisher_exclusion", true},
            {"publisher_identification", true}
        }}}}}
    };
    return routerRoles;
}

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

inline SessionId RouterContext::allocateSessionId() const
{
    auto r = router_.lock();
    if (r)
        return r->allocateSessionId();
    return 0;
}

inline ErrorOr<RealmContext>
RouterContext::join(std::shared_ptr<ServerSession> s)
{
    auto r = router_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->joinRemote(std::move(s));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERIMPL_HPP
