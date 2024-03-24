/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// HTTPS + Websocket Secure + WAMP server example
//******************************************************************************

#include <utility>
#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/httpsserver.hpp>
#include "../common/argsparser.hpp"
#include "../common/directtimeservice.hpp"
#include "../common/examplerouter.hpp"
#include "../common/sslserver.hpp"

//------------------------------------------------------------------------------
wamp::ServerOptions httpsOptions(uint_least16_t httpsPort)
{
    // These options are inherited by all blocks
    auto baseFileServerOptions =
        wamp::HttpFileServingOptions{}.withDocumentRoot("./www")
                                      .withCharset("utf-8");

    auto altFileServingOptions =
        wamp::HttpFileServingOptions{}.withDocumentRoot("./www-alt");

    auto mainRoute =
        wamp::HttpServeFiles{"/"}
            .withOptions(wamp::HttpFileServingOptions{}.withAutoIndex());

    auto altRoute =
        wamp::HttpServeFiles{"/alt"}
            .withAlias("/") // Substitutes "/alt" with "/"
                            // before appending to "./www-alt"
            .withOptions(altFileServingOptions);

    auto redirectRoute =
        wamp::HttpRedirect{"/wikipedia"}
            .withScheme("https")
            .withAuthority("en.wikipedia.org")
            .withAlias("/wiki") // Substitutes "/wikipedia" with "/wiki"
            .withStatus(wamp::HttpStatus::temporaryRedirect);

    auto wsRoute = wamp::HttpWebsocketUpgrade{"/time"};

    auto altBlockMainRoute =
        wamp::HttpServeFiles{"/"}.withOptions(altFileServingOptions);

    auto httpOptions =
        wamp::HttpServerOptions{}
            .withFileServingOptions(baseFileServerOptions)
            .addErrorPage({wamp::HttpStatus::notFound, "/notfound.html"});

    auto mainBlock =
        wamp::HttpServerBlock{}.addPrefixRoute(mainRoute)
                               .addExactRoute(altRoute)
                               .addPrefixRoute(redirectRoute)
                               .addExactRoute(wsRoute);

    auto altBlock =
        wamp::HttpServerBlock{"alt.localhost"}
            .addPrefixRoute(altBlockMainRoute);

    auto httpsEndpoint =
        wamp::HttpsEndpoint{httpsPort, &makeServerSslContext}
            .withOptions(httpOptions)
            .addBlock(mainBlock)
            .addBlock(altBlock);

    auto serverOptions =
        wamp::ServerOptions("https" + std::to_string(httpsPort),
                            std::move(httpsEndpoint),
                            wamp::jsonWithMaxDepth(10));

    return serverOptions;
}

//------------------------------------------------------------------------------
wamp::ServerOptions httpOptions(uint_least16_t httpPort,
                                uint_least16_t httpsPort)
{
    auto redirectRoute =
        wamp::HttpRedirect{"/"}
            .withScheme("https")
            .withPort(httpsPort)
            .withStatus(wamp::HttpStatus::temporaryRedirect);

    auto mainBlock = wamp::HttpServerBlock{}.addPrefixRoute(redirectRoute);

    auto httpEndpoint = wamp::HttpEndpoint{httpPort}.addBlock(mainBlock);

    auto serverOptions =
        wamp::ServerOptions("http" + std::to_string(httpPort),
                            std::move(httpEndpoint),
                            wamp::jsonWithMaxDepth(10));

    return serverOptions;
}

//------------------------------------------------------------------------------
// Usage: cppwamp-example-httpsserver [https_port [http_port [realm]]] | help
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    try
    {
        ArgsParser args{{{"https_port", "8443"},
                         {"http_port",  "8080"},
                         {"realm",      "cppwamp.examples"}}};

        uint_least16_t httpsPort = 0;
        uint_least16_t httpPort = 0;
        std::string realmUri;
        if (!args.parse(argc, argv, httpsPort, httpPort, realmUri))
            return 0;

        auto loggerOptions =
            wamp::utils::ConsoleLoggerOptions{}.withOriginLabel("router")
                                               .withColor();
        wamp::utils::ConsoleLogger logger{std::move(loggerOptions)};

        wamp::IoContext ioctx;

        wamp::Router router = initRouter(
            ioctx,
            {realmUri},
            {httpsOptions(httpsPort), httpOptions(httpsPort, httpPort)},
            logger);

        auto service = DirectTimeService::create(
            ioctx.get_executor(), router.realm(realmUri).value());
        service->start(router);

        runRouter(ioctx, router, logger);
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
