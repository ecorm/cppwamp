/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cppwamp/internal/timeoutscheduler.hpp>
#include <catch2/catch.hpp>

using namespace wamp;
using namespace wamp::internal;
namespace Matchers = Catch::Matchers;
using KeyList = std::vector<int>;

//------------------------------------------------------------------------------
TEST_CASE("Timeout Scheduler", "[TimeoutScheduler][Timeout]")
{
    struct Listener
    {
        KeyList& keys;
        void operator()(int key) {keys.push_back(key);}
    };

    auto ms =
        [](unsigned count) -> std::chrono::milliseconds
        {
            return std::chrono::milliseconds(count);
        };

    auto until =
        [](unsigned count) -> std::chrono::steady_clock::time_point
        {
            return std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(count);
        };

    IoContext ioctx;
    auto strand = boost::asio::make_strand(ioctx);
    auto deadlines = TimeoutScheduler<int>::create(strand);
    KeyList keys;
    Listener listener{keys};
    deadlines->listen(listener);

    SECTION("Single deadline")
    {
        deadlines->insert(42, ms(10));
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{42}));
    }

    SECTION("Multiple queued deadlines")
    {
        deadlines->insert(1, ms(10));
        deadlines->insert(3, ms(30));
        deadlines->insert(2, ms(20));
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{1, 2, 3}));
    }

    SECTION("Preempting enqueued deadline")
    {
        deadlines->insert(2, ms(20));
        deadlines->insert(1, ms(10));
        deadlines->insert(3, ms(30));
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{1, 2, 3}));
    }

    SECTION("Preempting dequeued deadline")
    {
        deadlines->insert(2, ms(20));
        deadlines->insert(3, ms(30));
        ioctx.run_until(until(5));
        deadlines->insert(1, ms(10));
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{1, 2, 3}));
    }

    SECTION("Erasing enqueued deadline")
    {
        deadlines->insert(1, ms(10));
        deadlines->insert(2, ms(20));
        deadlines->insert(3, ms(30));
        deadlines->erase(2);
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{1, 3}));
    }

    SECTION("Erasing dequeued deadline")
    {
        deadlines->insert(1, ms(10));
        deadlines->insert(2, ms(20));
        deadlines->insert(3, ms(30));
        ioctx.run_until(until(5));
        deadlines->erase(1);
        ioctx.restart();
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{2, 3}));
    }

    SECTION("Updating enqueued deadline")
    {
        deadlines->insert(1, ms(10));
        deadlines->insert(2, ms(20));
        deadlines->insert(3, ms(30));
        deadlines->update(2, ms(40));
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{1, 3, 2}));
    }

    SECTION("Preempting enqueued deadline via update")
    {
        deadlines->insert(1, ms(20));
        deadlines->insert(2, ms(30));
        deadlines->insert(3, ms(40));
        deadlines->update(2, ms(10));
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{2, 1, 3}));
    }

    SECTION("Preempting dequeued deadline via update")
    {
        deadlines->insert(1, ms(20));
        deadlines->insert(2, ms(30));
        deadlines->insert(3, ms(40));
        ioctx.run_until(until(5));
        deadlines->update(2, ms(10));
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{2, 1, 3}));
    }

    SECTION("Clearing enqueued deadlines")
    {
        deadlines->insert(1, ms(10));
        deadlines->insert(2, ms(20));
        deadlines->insert(3, ms(30));
        deadlines->clear();
        ioctx.run_until(until(40));
        CHECK(keys.empty());
    }

    SECTION("Clearing a dequeued deadline")
    {
        deadlines->insert(1, ms(10));
        deadlines->insert(2, ms(20));
        deadlines->insert(3, ms(30));
        ioctx.run_until(until(15));
        deadlines->clear();
        ioctx.restart();
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{1}));
    }

    SECTION("Muting the handler")
    {
        deadlines->insert(1, ms(10));
        deadlines->insert(2, ms(20));
        deadlines->insert(3, ms(30));
        ioctx.run_until(until(15));
        deadlines->unlisten();
        ioctx.restart();
        ioctx.run();
        CHECK_THAT(keys, Matchers::Equals(KeyList{1}));
    }
}
