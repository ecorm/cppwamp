/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <chrono>
#include <ctime>
#include <iostream>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/conversion.hpp>
#include <cppwamp/corosession.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/uds.hpp>

const std::string realm = "cppwamp.demo.time";
const std::string udsPath1 = "./.crossbar/uds-examples";
const std::string udsPath2 = "../.crossbar/uds-examples";

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
    AsioService iosvc;
    auto uds1 = connector<Msgpack>(iosvc, UdsPath(udsPath1));
    auto uds2 = connector<Msgpack>(iosvc, UdsPath(udsPath2));
    auto session = CoroSession<>::create(iosvc, {uds1, uds2});
    boost::asio::steady_timer timer(iosvc);

    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        session->connect(yield);
        session->join(Realm(realm), yield);
        session->enroll(Procedure("get_time"), basicRpc<std::tm>(&getTime),
                        yield);

        auto deadline = std::chrono::steady_clock::now();
        while (true)
        {
            deadline += std::chrono::seconds(1);
            timer.expires_at(deadline);
            timer.async_wait(yield);

            auto t = std::time(nullptr);
            const std::tm* local = std::localtime(&t);
            session->publish(Pub("time_tick").withArgs(*local), yield);
            std::cout << "Tick\n";
        }
    });

    iosvc.run();

    return 0;
}
