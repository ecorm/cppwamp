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
#include "../logging.hpp"
#include "../peerdata.hpp"
#include "../routerconfig.hpp"
#include "random.hpp"

namespace wamp
{

namespace internal
{

class ServerSession;
class RouterSession;
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
                      AccessLogHandler alh, AccessLogFilter alf)
    {
        return Ptr(new RouterLogger(std::move(s), std::move(lh), lv,
                                    std::move(alh), std::move(alf)));
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
        {
            assert(accessLogFilter_ != nullptr);
            bool allowed = accessLogFilter_(entry);
            if (allowed)
                postAny(strand_, accessLogHandler_, std::move(entry));
        }
    }

private:
    RouterLogger(IoStrand&& s, LogHandler&& lh, LogLevel lv,
                 AccessLogHandler&& alh, AccessLogFilter&& alf)
        : strand_(std::move(s)),
          logHandler_(std::move(lh)),
          accessLogHandler_(std::move(alh)),
          accessLogFilter_(std::move(alf)),
          logLevel_(lv)
    {}

    void setLevel(LogLevel level) {logLevel_.store(level);}

    IoStrand strand_;
    LogHandler logHandler_;
    AccessLogHandler accessLogHandler_;
    AccessLogFilter accessLogFilter_;
    std::atomic<LogLevel> logLevel_;

    friend class RouterImpl;
};

//------------------------------------------------------------------------------
class RealmContext
{
public:
    using RouterSessionPtr = std::shared_ptr<RouterSession>;

    RealmContext() = default;

    RealmContext(std::shared_ptr<RouterRealm> r);

    bool expired() const;

    explicit operator bool() const;

    IoStrand strand() const;

    RouterLogger::Ptr logger() const;

    void reset();

    void join(RouterSessionPtr s);

    void leave(SessionId sid);

    void subscribe(RouterSessionPtr s, Topic t);

    void unsubscribe(RouterSessionPtr s, SubscriptionId subId);

    void publish(RouterSessionPtr s, Pub pub);

    void enroll(RouterSessionPtr s, Procedure proc);

    void unregister(RouterSessionPtr s, RegistrationId rid);

    void call(RouterSessionPtr s, Rpc rpc);

    void cancelCall(RouterSessionPtr s, CallCancellation c);

    void yieldResult(RouterSessionPtr s, Result r);

    void yieldError(RouterSessionPtr s, Error e);

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

    ReservedId reserveSessionId();

    RealmContext realmAt(const String& uri) const;

private:
    std::weak_ptr<RouterImpl> router_;
    RandomIdPool::Ptr sessionIdPool_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
