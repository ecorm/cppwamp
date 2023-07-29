/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// WAMP router executable for running examples.
//******************************************************************************

#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcp.hpp>
#include <cppwamp/utils/consolelogger.hpp>

//------------------------------------------------------------------------------
int main()
{
    auto loggerOptions =
        wamp::utils::ConsoleLoggerOptions{}.withOriginLabel("router")
                                           .withColor();
    wamp::utils::ConsoleLogger logger{std::move(loggerOptions)};

    auto routerOptions = wamp::RouterOptions()
        .withLogHandler(logger)
        .withLogLevel(wamp::LogLevel::info)
        .withAccessLogHandler(wamp::AccessLogFilter(logger));

    auto realmOptions = wamp::RealmOptions("cppwamp.examples");

    auto serverOptions =
        wamp::ServerOptions("tcp12345", wamp::TcpEndpoint{12345},
                            wamp::jsonWithMaxDepth(10))
            .withAuthenticator(wamp::AnonymousAuthenticator::create());

    logger({wamp::LogLevel::info, "CppWAMP example router launched"});
    wamp::IoContext ioctx;

    wamp::Router router{ioctx, routerOptions};
    router.openRealm(realmOptions);
    router.openServer(serverOptions);

    ioctx.run();
    logger({wamp::LogLevel::info, "CppWAMP example router exit"});
    return 0;
}
