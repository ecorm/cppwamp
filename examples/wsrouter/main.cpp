/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// WAMP router executable for running Websocket example.
//******************************************************************************

#include <utility>
#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpserver.hpp>
#include <cppwamp/transports/websocketserver.hpp>
#include "../common/argsparser.hpp"
#include "../common/examplerouter.hpp"

//------------------------------------------------------------------------------
// Usage: cppwamp-example-wsrouter [ws_port [tcp_port [realm]]] | help
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    try
    {
        ArgsParser args{{{"ws_port",  "23456"},
                         {"tcp_port", "12345"},
                         {"realm",    "cppwamp.examples"}}};

        uint_least16_t wsPort = 0;
        uint_least16_t tcpPort = 0;
        std::string realm;
        if (!args.parse(argc, argv, wsPort, tcpPort, realm))
            return 0;

        auto loggerOptions =
            wamp::utils::ConsoleLoggerOptions{}.withOriginLabel("router")
                                               .withColor();
        wamp::utils::ConsoleLogger logger{std::move(loggerOptions)};

        auto tcpOptions =
            wamp::ServerOptions("tcp" + std::to_string(tcpPort),
                                wamp::TcpEndpoint{tcpPort},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        auto wsOptions =
            wamp::ServerOptions("ws" + std::to_string(wsPort),
                                wamp::WebsocketEndpoint{wsPort},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        wamp::IoContext ioctx;

        wamp::Router router = initRouter(
            ioctx,
            {realm},
            {std::move(tcpOptions), std::move(wsOptions)},
            logger);

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
