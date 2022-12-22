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
void onAuthenticate(wamp::AuthExchange::Ptr ex)
{
    ex->welcome();
}

//------------------------------------------------------------------------------
template <typename L>
wamp::RouterConfig routerConfig(L logger)
{
    return wamp::RouterConfig()
        .withLogHandler(logger)
        .withLogLevel(wamp::LogLevel::debug)
        .withAccessLogHandler(logger);
}

//------------------------------------------------------------------------------
wamp::RealmConfig realmConfig()
{
    return wamp::RealmConfig("cppwamp.demo.time");
}

//------------------------------------------------------------------------------
wamp::ServerConfig serverConfig()
{
    return wamp::ServerConfig("tcp12345",
                              wamp::TcpEndpoint{12345},
                              wamp::json);
//        .withAuthenticator(&onAuthenticate);
}

//------------------------------------------------------------------------------
int main()
{
    wamp::ColorConsoleLogger logger{"router"};
    logger({wamp::LogLevel::info, "CppWAMP Example Router launched"});
    wamp::IoContext ioctx;
    wamp::Router router{ioctx, routerConfig(logger)};
    router.addRealm(realmConfig());
    router.startServer(serverConfig());
    ioctx.run();
    logger({wamp::LogLevel::info, "CppWAMP Example Router exit"});
    return 0;
}
