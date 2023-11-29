/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using stackful coroutines.
//******************************************************************************

#include <chrono>
#include <ctime>
#include <iostream>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>

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
std::tm getTime()
{
    auto t = std::time(nullptr);
    return *std::localtime(&t);
}

//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    wamp::Session session(ioctx.get_executor());
    boost::asio::steady_timer timer(ioctx);

    wamp::spawn(ioctx,
        [tcp, &session, &timer](wamp::YieldContext yield)
        {
            session.connect(tcp, yield).value();
            session.join(realm, yield).value();
            session.enroll(wamp::Procedure("get_time"),
                           wamp::simpleRpc<std::tm>(&getTime),
                           yield).value();

            auto deadline = std::chrono::steady_clock::now();
            while (true)
            {
                deadline += std::chrono::seconds(1);
                timer.expires_at(deadline);
                timer.async_wait(yield);

                auto t = std::time(nullptr);
                const std::tm* local = std::localtime(&t);
                session.publish(wamp::Pub("time_tick").withArgs(*local),
                                yield).value();
                std::cout << "Tick: " << std::asctime(local) << "\n";
            }
        });

    ioctx.run();

    return 0;
}
