/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using stackful coroutines.
//******************************************************************************

#include <iostream>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
void onTimeTick(std::tm time)
{
    std::cout << "The current time is: " << std::asctime(&time);
}

//------------------------------------------------------------------------------
boost::asio::awaitable<void> client(wamp::Session& session, std::string realm,
                                    wamp::ConnectionWish where)
{
    using namespace wamp;
    using boost::asio::use_awaitable;

    (co_await session.connect(std::move(where), use_awaitable)).value();
    (co_await session.join(realm, use_awaitable)).value();

    auto result = (co_await session.call(Rpc("get_time"),
                                          use_awaitable)).value();
    auto time = result[0].to<std::tm>();
    std::cout << "The current time is: " << std::asctime(&time);

    (co_await session.subscribe("time_tick",
                                 wamp::simpleEvent<std::tm>(&onTimeTick),
                                 use_awaitable)).value();
}

//------------------------------------------------------------------------------
// Usage: cppwamp-example-coro20timeclient [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-coro20timeservice.
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
    boost::asio::co_spawn(ioctx, client(session, realm, tcp),
                          boost::asio::detached);
    ioctx.run();
    return 0;
}
