/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_TESTROUTER_HPP
#define CPPWAMP_TEST_TESTROUTER_HPP

#include <memory>
#include <cppwamp/accesslogging.hpp>

namespace test
{

//------------------------------------------------------------------------------
class Router
{
public:
    using AccessLogHandler = std::function<void (wamp::AccessLogEntry)>;

    struct AccessLogGuard
    {
        AccessLogGuard() :
            guard_(nullptr, [](int*){Router::instance().detachFromAccessLog();})
        {}

    private:
        std::shared_ptr<int> guard_;
    };

    static Router& instance();
    void start();
    void stop();
    AccessLogGuard attachToAccessLog(AccessLogHandler handler);
    void detachFromAccessLog();

private:
    Router();

    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace test

#endif // CPPWAMP_TEST_TESTROUTER_HPP
