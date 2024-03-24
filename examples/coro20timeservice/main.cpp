/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using C++20 coroutines.
//******************************************************************************

#include <chrono>
#include <iostream>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
std::tm getTime()
{
    auto t = std::time(nullptr);
    return *std::localtime(&t);
}

//------------------------------------------------------------------------------
boost::asio::awaitable<void> service(wamp::Session& session,
                                     std::string realm,
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
// Usage: cppwamp-example-coro20timeservice [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-coro20timeclient.
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ArgsParser args{{{"port", "12345"},
                     {"host", "localhost"},
                     {"realm", "cppwamp.examples"}}};

    std::string port, host, realm;
    if (!args.parse(argc, argv, port, host, realm))
        return 0;

    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(host, port).withFormat(wamp::json);
    wamp::Session session(ioctx);
    boost::asio::co_spawn(ioctx, service(session, realm, tcp),
                          boost::asio::detached);
    ioctx.run();
    return 0;
}
