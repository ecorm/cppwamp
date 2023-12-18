/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// WAMP router executable for running Websocket example.
//******************************************************************************

#include <csignal>
#include <utility>
#include <boost/asio/signal_set.hpp>
#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpserver.hpp>
#include <cppwamp/transports/websocketserver.hpp>
#include <cppwamp/utils/consolelogger.hpp>

//------------------------------------------------------------------------------
int main()
{
    try
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

        auto anonymousServerOptions =
            wamp::ServerOptions("tcp12345", wamp::TcpEndpoint{12345},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        auto ticketServerOptions =
            wamp::ServerOptions("ws23456", wamp::WebsocketEndpoint{23456},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        logger({wamp::LogLevel::info, "CppWAMP example router launched"});
        wamp::IoContext ioctx;

        wamp::Router router{ioctx, routerOptions};
        router.openRealm(realmOptions);
        router.openServer(anonymousServerOptions);
        router.openServer(ticketServerOptions);

        boost::asio::signal_set signals{ioctx, SIGINT, SIGTERM};
        signals.async_wait(
            [&router](const boost::system::error_code& ec, int sig)
            {
                if (ec)
                    return;
                const char* sigName = (sig == SIGINT)  ? "SIGINT" :
                                      (sig == SIGTERM) ? "SIGTERM" : "unknown";
                router.log({wamp::LogLevel::info,
                            std::string("Received ") + sigName + " signal"});
                router.close();
            });

        ioctx.run();
        logger({wamp::LogLevel::info, "CppWAMP example router exit"});
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unhandled exception: " << e.what() << ", terminating."
                  << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unhandled exception: <unknown>, terminating."
                  << std::endl;
    }

    return 0;
}
