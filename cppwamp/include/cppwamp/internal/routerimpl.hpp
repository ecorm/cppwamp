/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERIMPL_HPP
#define CPPWAMP_INTERNAL_ROUTERIMPL_HPP

#include <map>
#include <memory>
#include <string>
#include <utility>
#include "../routeroptions.hpp"
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
    using WeakPtr = std::shared_ptr<RouterImpl>;
    using Executor = AnyIoExecutor;

    static Ptr create(Executor exec, RouterOptions config)
    {
        config.initialize({});
        return Ptr(new RouterImpl(std::move(exec), std::move(config)));
    }

    // NOLINTNEXTLINE(bugprone-exception-escape)
    ~RouterImpl() {close(WampErrc::systemShutdown);}

    RouterRealm::Ptr addRealm(RealmOptions opts)
    {
        auto uri = opts.uri();
        RouterRealm::Ptr realm;

        {
            const MutexGuard guard(realmsMutex_);
            if (realms_.find(uri) == realms_.end())
            {
                realm = std::make_shared<RouterRealm>(
                    executor_, std::move(opts), options_,
                    RouterContext{shared_from_this()}, options_.rngFactory()());
                realms_.emplace(uri, realm);
            }
        }

        if (!realm)
        {
            alert("Rejected attempt to add realm "
                  "with duplicate URI '" + uri + "'");
        }
        else
        {
            inform("Adding realm '" + uri + "'");
        }

        return realm;
    }

    bool closeRealm(const Uri& uri, Abort reason)
    {
        RouterRealm::Ptr realm;

        {
            const MutexGuard guard(realmsMutex_);
            auto kv = realms_.find(uri);
            if (kv != realms_.end())
            {
                realm = std::move(kv->second);
                realms_.erase(kv);
            }
        }

        if (realm)
            realm->close(std::move(reason));
        else
            warn("Attempting to close non-existent realm named '" + uri + "'");

        return realm != nullptr;
    }

    RouterRealm::Ptr realmAt(const String& uri) const
    {
        const MutexGuard guard{realmsMutex_};
        auto found = realms_.find(uri);
        if (found == realms_.end())
            return nullptr;
        return found->second;
    }

    bool openServer(ServerOptions c)
    {
        RouterServer::Ptr server;
        auto name = c.name();

        {
            const MutexGuard guard(serversMutex_);
            if (servers_.find(name) == servers_.end())
            {
                server = RouterServer::create(
                    executor_, std::move(c), RouterContext{shared_from_this()});
                servers_.emplace(name, server);
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

    bool closeServer(const std::string& name, Abort reason)
    {
        RouterServer::Ptr server;

        {
            const MutexGuard guard(serversMutex_);
            auto kv = servers_.find(name);
            if (kv != servers_.end())
            {
                server = kv->second;
                servers_.erase(kv);
            }
        }

        if (server)
            server->close(std::move(reason));
        else
            warn("Attempting to close non-existent server named '" + name + "'");

        return server != nullptr;
    }

    // NOLINTNEXTLINE(bugprone-exception-escape)
    void close(Abort reason) noexcept
    {
        ServerMap servers;
        {
            const MutexGuard guard(serversMutex_);
            servers = std::move(servers_);
            servers_.clear();
        }

        RealmMap realms;
        {
            const MutexGuard guard(realmsMutex_);
            realms = std::move(realms_);
            realms_.clear();
        }

        if (!servers.empty() && !realms.empty())
        {
            auto msg = std::string("Shutting down router, with reason ") +
                       reason.uri();
            if (!reason.options().empty())
                msg += " " + toString(reason.options());
            inform(std::move(msg));
        }

        for (auto& kv: servers)
            kv.second->close(reason);

        for (auto& kv: realms)
            kv.second->close(reason);
    }

    LogLevel logLevel() const {return logger_->level();}

    void setLogLevel(LogLevel level) {logger_->setLevel(level);}

    void log(LogEntry entry) {logger_->log(std::move(entry));}

    const Executor& executor() const {return executor_;}

    RouterImpl(const RouterImpl&) = delete;
    RouterImpl(RouterImpl&&) = delete;
    RouterImpl& operator=(const RouterImpl&) = delete;
    RouterImpl& operator=(RouterImpl&&) = delete;

private:
    using MutexGuard = std::lock_guard<std::mutex>;
    using ServerMap = std::map<std::string, RouterServer::Ptr>;
    using RealmMap = std::map<Uri, RouterRealm::Ptr>;

    RouterImpl(Executor e, RouterOptions c)
        : options_(std::move(c)),
          executor_(e),
          logger_(std::make_shared<RouterLogger>(
              std::move(e), options_.logHandler(), options_.logLevel(),
              options_.accessLogHandler())),
          nextDirectSessionIndex_(0)
    {
        if (!options_.rngFactory())
            options_.withRngFactory(internal::DefaultPRNGFactory{});

        sessionIdPool_ = RandomIdPool::create(options_.rngFactory()());
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

    RouterLogger::Ptr logger() const {return logger_;}

    UriValidator::Ptr uriValidator() const {return options_.uriValidator();}

    ReservedId reserveSessionId()
    {
        return sessionIdPool_->reserve();
    }

    RealmContext realmContextAt(const String& uri) const
    {
        const MutexGuard guard{realmsMutex_};
        auto found = realms_.find(uri);
        if (found == realms_.end())
            return {};
        return RealmContext{found->second};
    }

    uint64_t nextDirectSessionIndex() {return ++nextDirectSessionIndex_;}

    ServerMap servers_;
    RealmMap realms_;
    RouterOptions options_;
    AnyIoExecutor executor_;
    std::mutex serversMutex_;
    mutable std::mutex realmsMutex_;
    RandomIdPool::Ptr sessionIdPool_;
    RouterLogger::Ptr logger_;
    std::atomic<uint64_t> nextDirectSessionIndex_;

    friend class RouterContext;
};


//******************************************************************************
// RouterContext
//******************************************************************************

inline RouterContext::RouterContext() = default;

inline RouterContext::RouterContext(const std::shared_ptr<RouterImpl>& r)
    : router_(r),
      sessionIdPool_(r->sessionIdPool_)
{}

inline bool RouterContext::expired() const {return router_.expired();}

inline RouterLogger::Ptr RouterContext::logger() const
{
    auto r = router_.lock();
    if (r)
        return r->logger();
    return nullptr;
}

inline UriValidator::Ptr RouterContext::uriValidator() const
{
    auto r = router_.lock();
    if (r)
        return r->uriValidator();
    return nullptr;
}

inline void RouterContext::reset() {router_.reset();}

inline ReservedId RouterContext::reserveSessionId()
{
    return sessionIdPool_->reserve();
}

inline RealmContext RouterContext::realmAt(const String& uri) const
{
    auto r = router_.lock();
    if (!r)
        return {};
    return r->realmContextAt(uri);
}

inline bool RouterContext::closeRealm(const String& uri, Abort reason)
{
    auto r = router_.lock();
    if (!r)
        return false;
    return r->closeRealm(uri, std::move(reason));

}

inline uint64_t RouterContext::nextDirectSessionIndex()
{
    auto r = router_.lock();
    if (!r)
        return 0;
    return r->nextDirectSessionIndex();
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERIMPL_HPP
