/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using C++20 coroutines.
//******************************************************************************
#include <chrono>
#include <ctime>
#include <iostream>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>

const std::string realm = "cppwamp.demo.time";
const std::string address = "localhost";
const short port = 54321u;

//------------------------------------------------------------------------------
namespace wamp
{
    // Convert a std::tm to/from an object variant.
    template <typename TConverter>
    void convert(TConverter& conv, std::tm& t)
    {
        conv ("sec",   t.tm_sec)
             ("min",   t.tm_min)
             ("hour",  t.tm_hour)
             ("mday",  t.tm_mday)
             ("mon",   t.tm_mon)
             ("year",  t.tm_year)
             ("wday",  t.tm_wday)
             ("yday",  t.tm_yday)
             ("isdst", t.tm_isdst);
    }
}

//------------------------------------------------------------------------------
std::tm getTime()
{
    auto t = std::time(nullptr);
    return *std::localtime(&t);
}

//------------------------------------------------------------------------------
boost::asio::awaitable<void> service(wamp::Session& session,
                                     wamp::ConnectionWish where)
{
    using namespace wamp;
    using boost::asio::use_awaitable;

    (co_await session.connect(std::move(where), use_awaitable)).value();
    (co_await session.join(realm, use_awaitable)).value();
    (co_await session.enroll("get_time", simpleRpc<std::tm>(&getTime),
                              use_awaitable)).value();

    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
    auto deadline = std::chrono::steady_clock::now();
    while (true)
    {
        deadline += std::chrono::seconds(1);
        timer.expires_at(deadline);
        co_await timer.async_wait(use_awaitable);

        auto t = std::time(nullptr);
        const std::tm* local = std::localtime(&t);
        (co_await session.publish(Pub("time_tick").withArgs(*local),
                                   use_awaitable)).value();
        std::cout << "Tick: " << std::asctime(local) << "\n";
    }
}

//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    wamp::Session session(ioctx);
    boost::asio::co_spawn(ioctx, service(session, tcp), boost::asio::detached);
    ioctx.run();
    return 0;
}
