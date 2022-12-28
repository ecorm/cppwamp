/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP router.
//******************************************************************************

#include <cppwamp/consolelogger.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/router.hpp>

//------------------------------------------------------------------------------
int main()
{
    wamp::ColorConsoleLogger logger{"router"};

    auto onAuthenticate = [&logger](wamp::AuthExchange::Ptr ex)
    {
        logger({
            wamp::LogLevel::debug,
            "main onAuthenticate: authid=" +
                ex->realm().authId().value_or("anonymous")});
        if (ex->challengeCount() == 0)
        {
            ex->challenge(wamp::Challenge("ticket").withChallenge("quest"),
                          std::string("memento"));
        }
        else if (ex->challengeCount() == 1)
        {
            logger({wamp::LogLevel::debug,
                    "memento = " +
                        wamp::any_cast<const std::string&>(ex->memento())});
            if (ex->authentication().signature() == "grail")
                ex->welcome({{"authrole", "admin"}});
            else
                ex->reject();
        }
        else
            ex->reject();
    };

    auto config = wamp::RouterConfig()
        .withLogHandler(logger)
        .withLogLevel(wamp::LogLevel::debug)
        .withAccessLogHandler(logger);

    auto realmConfig = wamp::RealmConfig("cppwamp.demo.time");

    auto serverConfig =
        wamp::ServerConfig("tcp12345", wamp::TcpEndpoint{12345}, wamp::json)
            .withAuthenticator(onAuthenticate);

    logger({wamp::LogLevel::info, "CppWAMP Example Router launched"});
    wamp::IoContext ioctx;
    wamp::Router router{ioctx, config};
    router.addRealm(realmConfig);
    router.startServer(serverConfig);
    ioctx.run();
    logger({wamp::LogLevel::info, "CppWAMP Example Router exit"});
    return 0;
}
