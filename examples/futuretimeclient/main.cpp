/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using std::future.
//******************************************************************************

#include <ctime>
#include <iostream>
#include <boost/asio/use_future.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/tcp.hpp>
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
    using boost::asio::use_future;

    // Run the io_context off in its own thread so that it operates
    // completely asynchronously with respect to the rest of the program.
    boost::asio::io_context ioctx;
    auto work = boost::asio::make_work_guard(ioctx);
    std::thread thread([&ioctx](){ ioctx.run(); });

    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    wamp::Session session(ioctx);

    // get() blocks the main thread until completion.
    // value() throws if there was an error.
    auto index = session.connect(std::move(tcp), use_future).get().value();
    std::cout << "Connected via " << index << std::endl;

    auto info = session.join(wamp::Petition(realm), use_future).get().value();
    std::cout << "Joined, SessionId=" << info.id() << std::endl;

    auto result = session.call(wamp::Rpc("get_time"),
                                use_future).get().value();
    auto time = result[0].to<std::tm>();
    std::cout << "The current time is: " << std::asctime(&time) << "\n";

    session.subscribe(wamp::Topic("time_tick"),
                       wamp::simpleEvent<std::tm>(&onTimeTick),
                       use_future).get().value();

    thread.join();

    return 0;
}
