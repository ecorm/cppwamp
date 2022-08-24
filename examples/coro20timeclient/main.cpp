/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using stackful coroutines.
//******************************************************************************

#include <ctime>
#include <iostream>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>

const std::string realm = "cppwamp.demo.time";
const std::string address = "localhost";
const short port = 12345u;

//------------------------------------------------------------------------------
namespace wamp
{
// Convert a std::tm to/from an object variant.
template <typename TConverter>
void convert(TConverter& conv, std::tm& t)
{
    conv("sec",   t.tm_sec)
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
void onTimeTick(std::tm time)
{
    std::cout << "The current time is: " << std::asctime(&time) << "\n";
}

//------------------------------------------------------------------------------
boost::asio::awaitable<void> client(wamp::Session& session,
                                    wamp::ConnectionWish where)
{
    using namespace wamp;
    using boost::asio::use_awaitable;

    (co_await session.connect(std::move(where), use_awaitable)).value();
    (co_await session.join(Realm(realm), use_awaitable)).value();

    auto result = (co_await session.call(Rpc("get_time"),
                                          use_awaitable)).value();
    auto time = result[0].to<std::tm>();
    std::cout << "The current time is: " << std::asctime(&time) << "\n";

    (co_await session.subscribe(Topic("time_tick"),
                                 wamp::simpleEvent<std::tm>(&onTimeTick),
                                 use_awaitable)).value();
}

//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    wamp::Session session(ioctx);
    boost::asio::co_spawn(ioctx, client(session, tcp), boost::asio::detached);
    ioctx.run();
    return 0;
}
