/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using stackful coroutines.
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
    wamp::Session session(ioctx);

    wamp::spawn(ioctx, [tcp, realm, &session](wamp::YieldContext yield)
    {
        session.connect(tcp, yield).value();
        session.join(realm, yield).value();
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
