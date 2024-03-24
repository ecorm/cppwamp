/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using std::future.
//******************************************************************************

#include <chrono>
#include <iostream>
#include <future>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_future.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
std::tm getTime()
{
    auto t = std::time(nullptr);
    return *std::localtime(&t);
}

//------------------------------------------------------------------------------
// Usage: cppwamp-example-futuretimeservice [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-futuretimeclient.
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
    boost::asio::steady_timer timer(ioctx);

    // get() blocks the main thread until completion.
    // value() throws if there was an error.
    auto index = session.connect(std::move(tcp), use_future).get().value();
    std::cout << "Connected via " << index << std::endl;

    auto welcome = session.join(realm, use_future).get().value();
    std::cout << "Joined, SessionId=" << welcome.sessionId() << std::endl;

    auto reg = session.enroll("get_time",
                              wamp::simpleRpc<std::tm>(&getTime),
                              use_future).get().value();
    std::cout << "Registered 'get_time', RegistrationId=" << reg.id()
              << std::endl;

    auto deadline = std::chrono::steady_clock::now();
    while (true)
    {
        deadline += std::chrono::seconds(1);
        timer.expires_at(deadline);
        timer.async_wait(use_future).get();

        auto t = std::time(nullptr);
        const std::tm* local = std::localtime(&t);
        session.publish(wamp::Pub("time_tick").withArgs(*local),
                         use_future).get().value();
        std::cout << "Tick: " << std::asctime(local);
    }

    return 0;
}
