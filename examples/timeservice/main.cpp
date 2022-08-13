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
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>

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
int main()
{
    using namespace wamp;
    AsioContext ioctx;
    auto tcp = connector<Json>(ioctx, TcpHost(address, port));
    auto session = Session::create(ioctx, tcp);
    boost::asio::steady_timer timer(ioctx);

    boost::asio::spawn(ioctx,
        [&session, &timer](boost::asio::yield_context yield)
        {
            session->connect(yield).value();
            session->join(Realm(realm), yield).value();
            session->enroll(Procedure("get_time"), simpleRpc<std::tm>(&getTime),
                            yield).value();

            auto deadline = std::chrono::steady_clock::now();
            while (true)
            {
                deadline += std::chrono::seconds(1);
                timer.expires_at(deadline);
                timer.async_wait(yield);

                auto t = std::time(nullptr);
                const std::tm* local = std::localtime(&t);
                session->publish(Pub("time_tick").withArgs(*local),
                                 yield).value();
                std::cout << "Tick: " << std::asctime(local) << "\n";
            }
        });

    ioctx.run();

    return 0;
}
