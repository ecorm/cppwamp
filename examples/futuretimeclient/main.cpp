/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using std::future.
//******************************************************************************

#include <iostream>
#include <boost/asio/use_future.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
void onTimeTick(std::tm time)
{
    std::cout << "The current time is: " << std::asctime(&time) << "\n";
}

//------------------------------------------------------------------------------
// Usage: cppwamp-example-futuretimeclient [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-futuretimeservice.
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ArgsParser args{{{"port", "12345"},
                     {"host", "localhost"},
                     {"realm", "cppwamp.examples"}}};

    std::string port, host, realm;
    if (!args.parse(argc, argv, port, host, realm))
        return 0;

    using boost::asio::use_future;

    // Run the io_context off in its own thread so that it operates
    // completely asynchronously with respect to the rest of the program.
    boost::asio::io_context ioctx;
    auto work = boost::asio::make_work_guard(ioctx);
    std::thread thread([&ioctx](){ ioctx.run(); });

    auto tcp = wamp::TcpHost(host, port).withFormat(wamp::json);
    wamp::Session session(ioctx);

    // get() blocks the main thread until completion.
    // value() throws if there was an error.
    auto index = session.connect(std::move(tcp), use_future).get().value();
    std::cout << "Connected via " << index << std::endl;

    auto welcome = session.join(realm, use_future).get().value();
    std::cout << "Joined, SessionId=" << welcome.sessionId() << std::endl;

    auto result = session.call(wamp::Rpc("get_time"),
                                use_future).get().value();
    auto time = result[0].to<std::tm>();
    std::cout << "The current time is: " << std::asctime(&time) << "\n";

    session.subscribe("time_tick",
                      wamp::simpleEvent<std::tm>(&onTimeTick),
                      use_future).get().value();

    thread.join();

    return 0;
}
