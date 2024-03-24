/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using stackful coroutines.
//******************************************************************************

#include <chrono>
#include <iostream>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
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
// Usage: cppwamp-example-timeclient [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-timeservice.
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ArgsParser args{{{"port", "12345"},
                     {"host", "localhost"},
                     {"realm", "cppwamp.examples"}}};

    std::string port, host, realm;
    if (!args.parse(argc, argv, port, host, realm))
        return 0;

    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(host, port).withFormat(wamp::json);
    wamp::Session session(ioctx.get_executor());
    boost::asio::steady_timer timer(ioctx);

    wamp::spawn(ioctx,
        [tcp, realm, &session, &timer](wamp::YieldContext yield)
        {
            session.connect(tcp, yield).value();
            session.join(realm, yield).value();
            session.enroll("get_time",
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
                std::cout << "Tick: " << std::asctime(local);
            }
        });

    ioctx.run();

    return 0;
}
