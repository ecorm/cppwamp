/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_TESTROUTER_HPP
#define CPPWAMP_TEST_TESTROUTER_HPP

#include <memory>
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
    using AccessLogHandler = std::function<void (wamp::AccessLogEntry)>;

    struct AccessLogSnoopGuard
    {
        ~AccessLogSnoopGuard() {RouterFixture::instance().unsnoopAccessLog();}
    };

    static RouterFixture& instance();
    static void cleanUp();
    static bool enabled();

    void start();
    void stop();
    AccessLogSnoopGuard snoopAccessLog(wamp::AnyCompletionExecutor exec,
                                       AccessLogHandler handler);
    wamp::Router& router();

private:
    struct Impl;

    RouterFixture();

    void unsnoopAccessLog();

    static std::shared_ptr<RouterFixture> theRouter_;
    static bool enabled_;
    std::shared_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
struct RouterLogLevelGuard
{
    explicit RouterLogLevelGuard(wamp::LogLevel level) : level_(level) {}

    ~RouterLogLevelGuard()
    {
        RouterFixture::instance().router().setLogLevel(level_);
    }

private:
    wamp::LogLevel level_;
};

//------------------------------------------------------------------------------
class ScopedRealm
{
public:
    ScopedRealm(wamp::Realm realm) : realm_(std::move(realm)) {}

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
