/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using stackful coroutines.
//******************************************************************************

#include <ctime>
#include <iostream>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/utils/consolelogger.hpp>

const std::string realm = "cppwamp.examples";
const std::string address = "localhost";
const short port = 12345u;

//------------------------------------------------------------------------------
int main()
{
    wamp::utils::ConsoleLogger logger;
    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    wamp::Session session(ioctx);
    session.listenLogged(logger);
    session.setLogLevel(wamp::LogLevel::trace);

    auto onChallenge = [](wamp::Challenge c)
    {
        std::cout << "challenge=" << c.challenge().value_or("none") << std::endl;
        c.authenticate({"grail"});
    };

    wamp::spawn(ioctx, [tcp, &session, onChallenge](wamp::YieldContext yield)
    {
        session.connect(tcp, yield).value();
        auto info = session.join(
            wamp::Realm(realm)
                .withAuthId("alice")
                .withAuthMethods({"ticket"}),
            onChallenge,
            yield).value();
        std::cout << info.authId().value_or("unknown") << std::endl;
        auto r = session.leave({"because.i.feel.like.it"}, yield);
        std::cout << r.value().uri() << std::endl;
        session.disconnect();
    });

    ioctx.run();

    return 0;
}
