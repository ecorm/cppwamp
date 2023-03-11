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
#include "localsessionimpl.hpp"
#include "random.hpp"
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

    ~RouterImpl() {close(WampErrc::systemShutdown);}

    bool addRealm(RealmConfig c)
    {
        auto uri = c.uri();
        bool exists = false;

        {
            MutexGuard lock(realmsMutex_);
            exists = realms_.find(uri) != realms_.end();
            if (!exists)
            {
                auto r = RouterRealm::create(
                    boost::asio::make_strand(executor_),
                    std::move(c),
                    config_,
                    {shared_from_this()});
                realms_.emplace(std::move(uri), std::move(r));
            }
        }

        if (exists)
        {
            alert("Rejected attempt to add realm "
                  "with duplicate URI '" + uri + "'");
        }
        else
        {
            inform("Adding realm '" + uri + "'");
        }

        return !exists;
    }

    bool closeRealm(const std::string& uri, Reason r)
    {
        RouterRealm::Ptr realm;

        {
            MutexGuard lock(realmsMutex_);
            auto kv = realms_.find(uri);
            if (kv != realms_.end())
            {
                realm = std::move(kv->second);
                realms_.erase(kv);
            }
        }

        if (realm)
            realm->close(std::move(r));
        else
            warn("Attempting to close non-existent realm named '" + uri + "'");

        return realm != nullptr;
    }

    bool openServer(ServerConfig c)
    {
        RouterServer::Ptr server;
        auto name = c.name();

        {
            MutexGuard lock(serversMutex_);
            if (servers_.find(name) != servers_.end())
            {
                server = RouterServer::create(executor_, std::move(c),
                                              {shared_from_this()});
                servers_.emplace(std::move(name), server);
            }
        }

        if (server)
        {
            server->start();
        }
        else
        {
            alert("Rejected attempt to open a server with duplicate name '" +
                  name + "'");
        }

        return server != nullptr;
    }

    bool closeServer(const std::string& name, Reason r)
    {
        RouterServer::Ptr server;

        {
            MutexGuard lock(serversMutex_);
            auto kv = servers_.find(name);
            if (kv != servers_.end())
            {
                server = kv->second;
                servers_.erase(kv);
            }
        }

        if (server)
            server->close(std::move(r));
        else
            warn("Attempting to close non-existent server named '" + name + "'");

        return server != nullptr;
    }

    LocalSessionImpl::Ptr localJoin(String realmUri, AuthInfo a,
                                    AnyCompletionExecutor e)
    {
        RouterRealm::Ptr realm;

        {
            MutexGuard lock(realmsMutex_);
            auto kv = realms_.find(realmUri);
            if (kv != realms_.end())
                realm = kv->second;
        }

        if (!realm)
            return nullptr;

        auto session = LocalSessionImpl::create(strand_, std::move(e));
        // session->join({std::move(realm)}, std::move(realmUri), std::move(a));
        return session;
    }

    void close(Reason r)
    {
        auto msg = std::string("Shutting down router, with reason ") + r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        inform(std::move(msg));

        ServerMap servers;
        {
            MutexGuard lock(serversMutex_);
            servers = std::move(servers_);
            servers_.clear();
        }

        RealmMap realms;
        {
            MutexGuard lock(realmsMutex_);
            realms = std::move(realms_);
            realms_.clear();
        }

        for (auto& kv: servers_)
            kv.second->close(r);

        for (auto& kv: realms_)
            kv.second->close(r);
    }

    const IoStrand& strand() const {return strand_;}

private:
    using MutexGuard = std::lock_guard<std::mutex>;
    using ServerMap = std::map<std::string, RouterServer::Ptr>;
    using RealmMap = std::map<std::string, RouterRealm::Ptr>;

    RouterImpl(Executor e, RouterConfig c)
        : config_(std::move(c)),
          executor_(std::move(e)),
          strand_(boost::asio::make_strand(executor_)),
          logger_(RouterLogger::create(strand_, config_.logHandler(),
                                       config_.logLevel(),
                                       config_.accessLogHandler()))
    {
        if (!config_.sessionRNG())
            config_.withSessionRNG(internal::DefaultPRNG64{});

        if (!config_.publicationRNG())
            config_.withPublicationRNG(internal::DefaultPRNG64{});

        sessionIdPool_ = RandomIdPool::create(config_.sessionRNG());
    }

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    void inform(String msg)
    {
        logger_->log({LogLevel::info, std::move(msg)});
    }

    void warn(String msg, std::error_code ec = {})
    {
        logger_->log({LogLevel::warning, std::move(msg), ec});
    }

    void alert(String msg, std::error_code ec = {})
    {
        logger_->log({LogLevel::error, std::move(msg), ec});
    }

    void log(LogEntry&& e) {logger_->log(std::move(e));}

    RouterLogger::Ptr logger() const {return logger_;}

    ReservedId reserveSessionId()
    {
        return sessionIdPool_->reserve();
    }

    RealmContext realmAt(const String& uri) const
    {
        MutexGuard lock(const_cast<std::mutex&>(realmsMutex_));

        auto found = realms_.find(uri);
        if (found == realms_.end())
            return {};
        return {found->second};
    }

    ServerMap servers_;
    RealmMap realms_;
    RouterConfig config_;
    AnyIoExecutor executor_;
    IoStrand strand_;
    std::mutex serversMutex_;
    std::mutex realmsMutex_;
    RandomIdPool::Ptr sessionIdPool_;
    RouterLogger::Ptr logger_;

    friend class RouterContext;
};


//******************************************************************************
// RouterContext
//******************************************************************************

inline RouterContext::RouterContext(std::shared_ptr<RouterImpl> r)
    : router_(r),
      sessionIdPool_(r->sessionIdPool_)
{}

inline RouterLogger::Ptr RouterContext::logger() const
{
    auto r = router_.lock();
    if (r)
        return r->logger();
    return nullptr;
}

inline ReservedId RouterContext::reserveSessionId()
{
    return sessionIdPool_->reserve();
}

inline RealmContext RouterContext::realmAt(const String& uri) const
{
    auto r = router_.lock();
    if (!r)
        return {};
    return r->realmAt(uri);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERIMPL_HPP
