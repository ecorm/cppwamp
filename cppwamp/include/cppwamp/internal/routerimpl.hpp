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
#include "localsessionimpl.hpp"
#include "routercontext.hpp"
#include "routerrealm.hpp"
#include "routerserver.hpp"

namespace wamp
{


namespace internal
{

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
        auto r = RouterRealm::create(strand_, std::move(c),
                                     {shared_from_this()});
        realms_.emplace(std::move(uri), std::move(r));
        return true;
    }

    bool closeRealm(const std::string& name, bool terminate, Reason r)
    {
        auto kv = realms_.find(name);
        if (kv == realms_.end())
        {
            log({LogLevel::warning,
                 "Attempting to close non-existent realm named '" + name + "'"});
            return false;
        }

        kv->second->close(terminate, std::move(r));
        realms_.erase(kv);
        return true;
    }

    bool openServer(ServerConfig c)
    {
        auto name = c.name();
        if (servers_.find(name) != servers_.end())
        {
            log({LogLevel::warning,
                 "Rejected attempt to open a server with duplicate name '" + name + "'"});
            return false;
        }

        using std::move;
        auto s = RouterServer::create(strand_, move(c), {shared_from_this()});
        servers_.emplace(std::move(name), s);
        s->start();
        return true;
    }

    bool closeServer(const std::string& name, bool terminate, Reason r)
    {
        auto kv = servers_.find(name);
        if (kv == servers_.end())
        {
            log({LogLevel::warning,
                 "Attempting to close non-existent server named '" + name + "'"});
            return false;
        }

        kv->second->close(terminate, std::move(r));
        servers_.erase(kv);
        return true;
    }

    LocalSessionImpl::Ptr localJoin(String realmUri, AuthInfo a,
                                    AnyCompletionExecutor e)
    {
        auto kv = realms_.find(realmUri);
        if (kv == realms_.end())
            return nullptr;

        auto realm = kv->second;
        a.join({}, std::move(realmUri), sessionIdPool_.allocate(),
               RouterContext::roles());
        auto session = LocalSessionImpl::create({realm}, std::move(a),
                                                std::move(e));
        realm->join(session);
        return session;
    }

    void close(bool terminate, Reason r)
    {
        // TODO: Check already closed
        std::string msg = terminate ? "Shutting down router, with reason "
                                    : "Terminating router, with reason ";
        msg += r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        log({LogLevel::info, std::move(msg)});

        for (auto& kv: servers_)
            kv.second->close(terminate, r);
        servers_.clear();
        for (auto& kv: realms_)
            kv.second->close(terminate, r);
        realms_.clear();
        // TODO: Wait for GOODBYE acknowledgements or timeout
    }

    const IoStrand& strand() const {return strand_;}

private:
    RouterImpl(Executor e, RouterConfig c)
        : strand_(boost::asio::make_strand(e)),
          sessionIdPool_(c.sessionIdSeed()),
          logger_(RouterLogger::create(strand_, c.logHandler(), c.logLevel(),
                                       c.accessLogHandler())),
          config_(std::move(c))
    {}

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    void log(LogEntry&& e) {logger_->log(std::move(e));}

    RouterLogger::Ptr logger() const {return logger_;}

    void freeSessionId(SessionId id)
    {
        return sessionIdPool_.free(id);
    }

    bool realmExists(const String& uri) const
    {
        return realms_.find(uri) != realms_.end();
    }

    ErrorOr<RealmContext> joinRemote(const String& realmUri,
                                     ServerSession::Ptr s)
    {
        auto kv = realms_.find(realmUri);
        if (kv == realms_.end())
            return makeUnexpectedError(SessionErrc::noSuchRealm);
        auto realm = kv->second;
        s->setWampSessionId(sessionIdPool_.allocate());
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

inline void RouterContext::freeSessionId(SessionId id) const
{
    if (id == nullId())
        return;
    auto r = router_.lock();
    if (r)
        r->freeSessionId(id);
}

inline bool RouterContext::realmExists(const String& uri) const
{
    auto r = router_.lock();
    if (!r)
        return false;
    return r->realmExists(uri);
}

inline ErrorOr<RealmContext>
RouterContext::join(const String& realmUri, std::shared_ptr<ServerSession> s)
{
    auto r = router_.lock();
    if (!r)
        return makeUnexpectedError(SessionErrc::noSuchRealm);
    return r->joinRemote(realmUri, std::move(s));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERIMPL_HPP
