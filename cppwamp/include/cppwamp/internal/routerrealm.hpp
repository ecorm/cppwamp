/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <map>
#include <memory>
#include <thread>
#include <string>
#include <utility>
#include "localsessionimpl.hpp"
#include "routercontext.hpp"
#include "routerserver.hpp"

namespace wamp
{


namespace internal
{

class LocalSessionImpl;

//------------------------------------------------------------------------------
class RouterRealm : public std::enable_shared_from_this<RouterRealm>
{
public:
    using Ptr = std::shared_ptr<RouterRealm>;
    using Executor = AnyIoExecutor;
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    static Ptr create(Executor exec, RouterContext router, std::string uri)
    {
        using std::move;
        return Ptr(new RouterRealm(move(exec), move(router), move(uri)));
    }

    const IoStrand& strand() const {return strand_;}

    const std::string& uri() const {return uri_;}

    void join(LocalSessionImpl::Ptr session)
    {
        auto id = session->id();
        localSessions_.emplace(id, std::move(session));
    }

    void join(ServerSession::Ptr session)
    {
        auto id = session->id();
        serverSessions_.emplace(id, std::move(session));
    }

private:
    RouterRealm(Executor exec, RouterContext router, std::string uri)
        : strand_(boost::asio::make_strand(exec)),
          router_(std::move(router)),
          uri_(std::move(uri)),
          logger_(router_.logger())
    {}

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    RouterLogger::Ptr logger() const {return logger_;}

    template <typename TSessionLike>
    void safeLeave(std::shared_ptr<TSessionLike> s)
    {
        struct Dispatched
        {
            Ptr self;
            std::shared_ptr<TSessionLike> s;
            void operator()() {self->leave(std::move(s));}
        };

        dispatch(Dispatched{shared_from_this(), std::move(s)});
    }

    void leave(LocalSessionImpl::Ptr session)
    {
        // TODO
        localSessions_.erase(session->id());
    }

    void leave(ServerSession::Ptr session)
    {
        // TODO
        serverSessions_.erase(session->id());
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
    RouterContext router_;
    std::map<SessionId, LocalSessionImpl::Ptr> localSessions_;
    std::map<SessionId, ServerSession::Ptr> serverSessions_;
    std::string uri_;
    RouterLogger::Ptr logger_;

    // TODO: Consider common interface for local and server sessions

    friend class RealmContext;
};


//******************************************************************************
// RealmContext
//******************************************************************************

inline RealmContext::RealmContext(std::shared_ptr<RouterRealm> r)
    : realm_(std::move(r))
{}

inline IoStrand RealmContext::strand() const
{
    auto r = realm_.lock();
    if (r)
        return r->strand();
    return {};
}

inline RouterLogger::Ptr RealmContext::logger() const
{
    auto r = realm_.lock();
    if (r)
        return r->logger();
    return {};
}

inline void RealmContext::onMessage(std::shared_ptr<ServerSession> s,
                                    WampMessage m)
{
    auto r = realm_.lock();
    if (r)
        r->safeOnMessage(std::move(s), std::move(m));
}

inline void RealmContext::leave(std::shared_ptr<LocalSessionImpl> s)
{
    auto r = realm_.lock();
    if (r)
        r->safeLeave(std::move(s));
}

inline void RealmContext::leave(std::shared_ptr<ServerSession> s)
{
    auto r = realm_.lock();
    if (r)
        r->safeLeave(std::move(s));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
