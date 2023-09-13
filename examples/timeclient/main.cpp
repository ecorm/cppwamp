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
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>

const std::string realm = "cppwamp.examples";
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
    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    wamp::Session session(ioctx);

    wamp::spawn(ioctx, [tcp, &session](wamp::YieldContext yield)
    {
        session.connect(tcp, yield).value();
        session.join(wamp::Petition(realm), yield).value();
        auto result = session.call(wamp::Rpc("get_time"), yield).value();
        auto time = result[0].to<std::tm>();
        std::cout << "The current time is: " << std::asctime(&time) << "\n";

        session.subscribe(wamp::Topic("time_tick"),
                          wamp::simpleEvent<std::tm>(&onTimeTick),
                          yield).value();
    });

    ioctx.run();

    return 0;
}
