/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app that authenticates.
//******************************************************************************

#include <iostream>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include <cppwamp/unpacker.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
void onTimeTick(std::tm time)
{
    std::cout << "The current time is: " << std::asctime(&time);
}

//------------------------------------------------------------------------------
// Usage:
// cppwamp-example-timeclientauth [username [password [port [host [realm]]]]]
//                                | help
// Use with cppwamp-example-router and cppwamp-example-timeservice.
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ArgsParser args{{{"username", "alice"},
                     {"password", "password123"},
                     {"port", "12345"},
                     {"host", "localhost"},
                     {"realm", "cppwamp.examples"}}};

    std::string username, password, port, host, realm;
    if (!args.parse(argc, argv, username, password, port, host, realm))
        return 0;

    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(host, port).withFormat(wamp::json);
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
        std::cout << "The current time is: " << std::asctime(&time);

        session.subscribe("time_tick",
                          wamp::simpleEvent<std::tm>(&onTimeTick),
                          yield).value();
    });

    ioctx.run();

    return 0;
}
