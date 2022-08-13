/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using stackful coroutines.
//******************************************************************************

#include <ctime>
#include <iostream>
#include <boost/asio/spawn.hpp>
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
void onTimeTick(std::tm time)
{
    std::cout << "The current time is: " << std::asctime(&time) << "\n";
}

//------------------------------------------------------------------------------
int main()
{
    using namespace wamp;

    AsioContext ioctx;
    auto tcp = connector<Json>(ioctx, TcpHost(address, port));
    auto session = Session::create(ioctx, tcp);

    boost::asio::spawn(ioctx, [&session](boost::asio::yield_context yield)
    {
        session->connect(yield).value();
        session->join(Realm(realm), yield).value();

        auto result = session->call(Rpc("get_time"), yield).value();
        auto time = result[0].to<std::tm>();
        std::cout << "The current time is: " << std::asctime(&time) << "\n";

        session->subscribe(Topic("time_tick"),
                           wamp::simpleEvent<std::tm>(&onTimeTick),
                           yield).value();
    });

    ioctx.run();

    return 0;
}
