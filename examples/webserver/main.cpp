/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// HTTP + Websocket + WAMP support server example
//******************************************************************************

#include <csignal>
#include <utility>
#include <boost/asio/signal_set.hpp>
#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/httpserver.hpp>
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

        // These options are inherited by all blocks
        auto baseFileServerOptions =
            wamp::HttpFileServingOptions{}.withDocumentRoot("./www")
                                          .withCharset("utf-8");

        auto altFileServingOptions =
            wamp::HttpFileServingOptions{}
                .withDocumentRoot("./www-alt");

        auto mainRoute =
            wamp::HttpServeFiles{"/"}
                .withOptions(wamp::HttpFileServingOptions{}.withAutoIndex());

        auto altRoute =
            wamp::HttpServeFiles{"/alt"}
                .withAlias("/") // Substitutes "/alt" with "/"
                                // before appending to "./www-alt"
                .withOptions(altFileServingOptions);

        auto wsRoute = wamp::HttpWebsocketUpgrade{"/time"};

        auto altBlockMainRoute =
            wamp::HttpServeFiles{"/"}
                .withOptions(altFileServingOptions);

        auto httpOptions =
            wamp::HttpServerOptions{}
                .withFileServingOptions(baseFileServerOptions)
                .addErrorPage({wamp::HttpStatus::notFound, "/notfound.html"});

        auto mainBlock =
            wamp::HttpServerBlock{}.addPrefixRoute(mainRoute)
                                   .addExactRoute(altRoute)
                                   .addExactRoute(wsRoute);

        auto altBlock =
            wamp::HttpServerBlock{"alt.localhost"}
                .addPrefixRoute(altBlockMainRoute);

        auto httpEndpoint =
            wamp::HttpEndpoint{8080}.withOptions(httpOptions)
                                    .addBlock(mainBlock)
                                    .addBlock(altBlock);

        auto serverOptions =
            wamp::ServerOptions("http8080", std::move(httpEndpoint),
                                wamp::jsonWithMaxDepth(10));

        logger({wamp::LogLevel::info, "CppWAMP example web server launched"});
        wamp::IoContext ioctx;

        wamp::Router router{ioctx, routerOptions};
        router.openRealm(realmOptions);
        router.openServer(serverOptions);

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
        logger({wamp::LogLevel::info, "CppWAMP example web server exit"});
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
