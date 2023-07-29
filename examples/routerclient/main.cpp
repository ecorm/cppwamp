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
#include <cppwamp/transports/tcp.hpp>
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
    session.observeIncidents(
        [&logger](wamp::Incident i) {logger(i.toLogEntry());});
    session.enableTracing();
    int eventCount = 0;

    auto onChallenge = [](wamp::Challenge c)
    {
        c.authenticate({"grail"});
    };

    auto onEvent = [&logger, &eventCount](wamp::Event ev)
    {
        logger({wamp::LogLevel::debug,
                "Event - " + ev.args().at(0).as<wamp::String>()});
        ++eventCount;
    };

    auto echo = [](wamp::Invocation inv) -> wamp::Outcome
    {
        return wamp::Result{inv.args().at(0).as<wamp::String>()};
    };

    wamp::spawn(ioctx, [&](wamp::YieldContext yield)
    {
        session.connect(tcp, yield).value();
        session.join(
            wamp::Petition(realm)
                .withAuthId("alice")
                .withAuthMethods({"ticket"}),
            onChallenge,
            yield).value();
        session.enroll(wamp::Procedure{"echo"}, echo, yield).value();
        session.subscribe(wamp::Topic{"foo"}, onEvent, yield).value();
        auto echoed = session.call(wamp::Rpc{"local_echo"}.withArgs("hello"),
                                   yield);
        logger({wamp::LogLevel::debug,
                "echoed=" + wamp::toString(echoed.value().args())});
        session.publish(wamp::Pub{"foo"}.withArgs("bar").withExcludeMe(false),
                        yield).value();

        while (eventCount == 0)
            boost::asio::post(ioctx, yield);

        session.leave(yield).value();
        session.disconnect();
    });

    ioctx.run();

    return 0;
}
