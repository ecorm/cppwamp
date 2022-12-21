/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
#define CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP

#include <atomic>
#include <memory>
#include "../anyhandler.hpp"
#include "../asiodefs.hpp"
#include "../erroror.hpp"
#include "../logging.hpp"
#include "wampmessage.hpp"

namespace wamp
{

namespace internal
{

class LocalSessionImpl;
class ServerSession;
class RouterRealm;
class RouterImpl;

//------------------------------------------------------------------------------
class RouterLogger : public std::enable_shared_from_this<RouterLogger>
{
public:
    using Ptr = std::shared_ptr<RouterLogger>;
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    static Ptr create(IoStrand s, LogHandler f, LogLevel lv)
    {
        return Ptr(new RouterLogger(std::move(s), std::move(f), lv));
    }

    LogLevel level() const {return logLevel_.load();}

    void log(LogEntry entry)
    {
        if (entry.severity() >= level())
            dispatchVia(strand_, handler_, std::move(entry));
    }

private:
    RouterLogger(IoStrand&& s, LogHandler&& f, LogLevel lv)
        : strand_(std::move(s)),
          handler_(std::move(f)),
          logLevel_(lv)
    {}

    void setLevel(LogLevel level) {logLevel_.store(level);}

    IoStrand strand_;
    LogHandler handler_;
    std::atomic<LogLevel> logLevel_;

    friend class RouterImpl;
};

//------------------------------------------------------------------------------
class RealmContext
{
public:
    RealmContext() = default;
    RealmContext(std::shared_ptr<RouterRealm> r);
    bool expired() const;
    IoStrand strand() const;
    RouterLogger::Ptr logger() const;
    void onMessage(std::shared_ptr<ServerSession> s, WampMessage m);
    void leave(std::shared_ptr<LocalSessionImpl> s);
    void leave(std::shared_ptr<ServerSession> s);
    void reset();

private:
    std::weak_ptr<RouterRealm> realm_;
};

//------------------------------------------------------------------------------
class RouterContext
{
public:
    static const Object& roles();
    RouterContext(std::shared_ptr<RouterImpl> r);
    RouterLogger::Ptr logger() const;
    SessionId allocateSessionId() const;
    ErrorOr<RealmContext> join(std::shared_ptr<ServerSession> s);

private:
    std::weak_ptr<RouterImpl> router_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
