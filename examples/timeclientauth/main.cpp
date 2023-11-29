/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app that authenticates.
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
const short port = 23456u;

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
// Command line usage: timeclientauth [username [password]]
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    const char* username = argc >= 2 ? argv[1] : "alice";
    const char* password = argc >= 3 ? argv[2] : "password123";

    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    wamp::Session session(ioctx);

    auto onChallenge = [password](wamp::Challenge challenge)
    {
        challenge.authenticate(wamp::Authentication{password});
    };

    wamp::spawn(ioctx, [&](wamp::YieldContext yield)
    {
        session.connect(tcp, yield).value();
        auto hello = wamp::Hello{realm}.withAuthMethods({"ticket"})
                                       .withAuthId(username);
        auto welcome = session.join(hello, onChallenge, yield);
        if (!welcome.has_value())
        {
            std::cerr << "Login failed: " << welcome.error().message() << "\n";
            session.disconnect();
            return;
        }

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
