/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXAMPLES_EXAMPLEROUTER_HPP
#define CPPWAMP_EXAMPLES_EXAMPLEROUTER_HPP

#include <csignal>
#include <utility>
#include <vector>
#include <boost/asio/signal_set.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/utils/consolelogger.hpp>

//------------------------------------------------------------------------------
wamp::Router initRouter(wamp::IoContext& ioctx,
                        std::vector<wamp::RealmOptions> realms,
                        std::vector<wamp::ServerOptions> servers,
                        wamp::utils::ConsoleLogger& logger)
{
    auto routerOptions =
        wamp::RouterOptions()
            .withLogHandler(logger)
            .withLogLevel(wamp::LogLevel::info)
            .withAccessLogHandler(wamp::AccessLogFilter(logger));

    logger({wamp::LogLevel::info, "Router launched"});

    wamp::Router router{ioctx, routerOptions};
    for (auto& realm: realms)
        router.openRealm(std::move(realm)).value();
    for (auto& server: servers)
        router.openServer(std::move(server));

    return router;
}

//------------------------------------------------------------------------------
void runRouter(wamp::IoContext& ioctx, wamp::Router& router,
               wamp::utils::ConsoleLogger& logger)
{
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
    logger({wamp::LogLevel::info, "Router exit"});
}

#endif // CPPWAMP_EXAMPLES_EXAMPLEROUTER_HPP
