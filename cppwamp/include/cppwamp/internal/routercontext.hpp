/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
#define CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP

#include <atomic>
#include <cassert>
#include <memory>
#include "../anyhandler.hpp"
#include "../asiodefs.hpp"
#include "../errorinfo.hpp"
#include "../logging.hpp"
#include "../pubsubinfo.hpp"
#include "../rpcinfo.hpp"
#include "commandinfo.hpp"
#include "random.hpp"

namespace wamp
{

namespace internal
{

class ServerSession;
class RealmSession;
class RouterRealm;
class RouterImpl;

//------------------------------------------------------------------------------
class RouterLogger : public std::enable_shared_from_this<RouterLogger>
{
public:
    using Ptr = std::shared_ptr<RouterLogger>;
    using LogHandler = AnyReusableHandler<void (LogEntry)>;
    using AccessLogHandler = AnyReusableHandler<void (AccessLogEntry)>;

    static Ptr create(IoStrand s, LogHandler lh, LogLevel lv,
                      AccessLogHandler alh)
    {
        return Ptr(new RouterLogger(std::move(s), std::move(lh), lv,
                                    std::move(alh)));
    }

    LogLevel level() const {return logLevel_.load();}

    void log(LogEntry entry)
    {
        if (logHandler_ && entry.severity() >= level())
            postAny(strand_, logHandler_, std::move(entry));
    }

    void log(AccessLogEntry entry)
    {
        if (accessLogHandler_)
            postAny(strand_, accessLogHandler_, std::move(entry));
    }

private:
    RouterLogger(IoStrand&& s, LogHandler&& lh, LogLevel lv,
                 AccessLogHandler&& alh)
        : strand_(std::move(s)),
          logHandler_(std::move(lh)),
          accessLogHandler_(std::move(alh)),
          logLevel_(lv)
    {}

    void setLevel(LogLevel level) {logLevel_.store(level);}

    IoStrand strand_;
    LogHandler logHandler_;
    AccessLogHandler accessLogHandler_;
    std::atomic<LogLevel> logLevel_;

    friend class RouterImpl;
};

//------------------------------------------------------------------------------
class RealmContext
{
public:
    using RealmSessionPtr = std::shared_ptr<RealmSession>;

    RealmContext() = default;

    RealmContext(std::shared_ptr<RouterRealm> r);

    bool expired() const;

    RouterLogger::Ptr logger() const;

    void reset();

    bool join(RealmSessionPtr s);

    bool leave(SessionId sid);

    template <typename TCommand>
    bool send(RealmSessionPtr s, TCommand&& cmd);

private:
    std::weak_ptr<RouterRealm> realm_;
};

//------------------------------------------------------------------------------
class RouterContext
{
public:
    RouterContext();

    RouterContext(std::shared_ptr<RouterImpl> r);

    RouterLogger::Ptr logger() const;

    ReservedId reserveSessionId();

    RealmContext realmAt(const String& uri) const;

private:
    std::weak_ptr<RouterImpl> router_;
    RandomIdPool::Ptr sessionIdPool_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
