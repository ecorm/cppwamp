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
#include "routercontext.hpp"
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

    // Definition is below class declaration.
    std::shared_ptr<LocalSessionImpl>
    join(const std::string& realmUri, std::string authId,
         AnyCompletionExecutor fallbackExecutor);

    LogLevel logLevel() const {return config_.logLevel();}

    void log(LogEntry entry)
    {
        if (entry.severity() >= config_.logLevel())
            dispatchVia(strand_, config_.logHandler(), std::move(entry));
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

private:
    RouterImpl(Executor exec, RouterConfig config)
        : strand_(boost::asio::make_strand(exec)),
        config_(std::move(config))
    {}

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    void safeAddSession(std::shared_ptr<ServerSession> s)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<ServerSession> s;
            void operator()() {self->addSession(std::move(s));}
        };

        dispatch(Dispatched{shared_from_this(), std::move(s)});
    }

    void addSession(std::shared_ptr<ServerSession> session)
    {
        SessionId id = 0;
        do
        {
            id = sessionIdGenerator_();
        }
        while (sessions_.find(id) != sessions_.end());

        sessions_.emplace(id, session);
        session->start(id);
    }

    void safeRemoveSession(std::shared_ptr<ServerSession> s)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<ServerSession> s;
            void operator()() {self->removeSession(std::move(s));}
        };

        dispatch(Dispatched{shared_from_this(), std::move(s)});
    }

    void removeSession(std::shared_ptr<ServerSession> session)
    {
        // TODO
    }

    void safeOnMessage(std::shared_ptr<ServerSession> s, WampMessage m)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<ServerSession> s;
            WampMessage m;

            void operator()()
            {
                self->onMessage(std::move(s), std::move(m));
            }
        };

        dispatch(Dispatched{shared_from_this(), std::move(s), std::move(m)});
    }

    void onMessage(std::shared_ptr<ServerSession> session, WampMessage msg)
    {
        // TODO
    }

    IoStrand strand_;
    RouterConfig config_;
    std::map<std::string, Server::Ptr> servers_;
    std::map<SessionId, ServerSession::Ptr> sessions_;
    RandomIdGenerator sessionIdGenerator_;

    friend class RouterContext;
};


//******************************************************************************
// RouterContext
//******************************************************************************

inline RouterContext::RouterContext(std::shared_ptr<RouterImpl> r)
    : router_(std::move(r))
{}

inline LogLevel RouterContext::logLevel() const
{
    auto r = router_.lock();
    return r ? r->logLevel() : LogLevel::off;
}

inline void RouterContext::addSession(std::shared_ptr<ServerSession> s)
{
    auto r = router_.lock();
    if (r)
        r->safeAddSession(std::move(s));
}

inline void RouterContext::removeSession(std::shared_ptr<ServerSession> s)
{
    auto r = router_.lock();
    if (r)
        r->safeRemoveSession(std::move(s));
}

inline void RouterContext::onMessage(std::shared_ptr<ServerSession> s,
                                     WampMessage m)
{
    auto r = router_.lock();
    if (r)
        r->safeOnMessage(std::move(s), std::move(m));
}

inline void RouterContext::log(LogEntry e)
{
    auto r = router_.lock();
    if (r)
        r->log(std::move(e));
}

} // namespace internal

} // namespace wamp


#include "localsessionimpl.hpp"

namespace wamp
{

namespace internal
{

inline std::shared_ptr<LocalSessionImpl>
RouterImpl::join(const std::string& realmUri, std::string authId,
                 AnyCompletionExecutor fallbackExecutor)
{
    return nullptr;
}

} // namespace internal
} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERIMPL_HPP
