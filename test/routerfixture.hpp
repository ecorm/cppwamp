/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_TESTROUTER_HPP
#define CPPWAMP_TEST_TESTROUTER_HPP

#include <memory>
#include <thread>
#include <cppwamp/accesslogging.hpp>
#include <cppwamp/realm.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/spawn.hpp>

namespace test
{

//------------------------------------------------------------------------------
class RouterFixture
{
public:
    using LogHandler = std::function<void (wamp::LogEntry)>;
    using AccessLogHandler = std::function<void (wamp::AccessLogEntry)>;

    struct LogLevelGuard
    {
        explicit LogLevelGuard(wamp::LogLevel level) : level_(level) {}

        ~LogLevelGuard()
        {
            // Allow time for realm to close before restoring log level.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            RouterFixture::instance().restoreLogLevel(level_);
        }

    private:
        wamp::LogLevel level_;
    };

    struct LogSnoopGuard
    {
        ~LogSnoopGuard() {RouterFixture::instance().unsnoopLog();}
    };

    struct AccessLogSnoopGuard
    {
        ~AccessLogSnoopGuard() {RouterFixture::instance().unsnoopAccessLog();}
    };

    static RouterFixture& instance();
    static void cleanUp();
    static bool enabled();

    void start();
    void stop();
    LogLevelGuard supressLogLevel(wamp::LogLevel level);
    LogSnoopGuard snoopLog(wamp::AnyCompletionExecutor exec,
                           LogHandler handler);
    AccessLogSnoopGuard snoopAccessLog(wamp::AnyCompletionExecutor exec,
                                       AccessLogHandler handler);
    wamp::Router& router();

private:
    struct Impl;

    RouterFixture();

    void restoreLogLevel(wamp::LogLevel level);
    void unsnoopLog();
    void unsnoopAccessLog();

    static std::shared_ptr<RouterFixture> theRouter_;
    static bool enabled_;
    std::shared_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
class ScopedRealm
{
public:
    ScopedRealm(wamp::Realm realm) : realm_(std::move(realm)) {}

    wamp::Realm* operator->() {return &realm_;}

    const wamp::Realm* operator->() const {return &realm_;}

    ~ScopedRealm() {realm_.close();}

private:
    wamp::Realm realm_;
};

//------------------------------------------------------------------------------
inline void suspendCoro(wamp::YieldContext& yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

} // namespace test

#endif // CPPWAMP_TEST_TESTROUTER_HPP
