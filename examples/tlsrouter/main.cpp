/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
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
#include <cppwamp/transports/tlsserver.hpp>
#include <cppwamp/utils/consolelogger.hpp>

//------------------------------------------------------------------------------
wamp::SslContext makeSslContext()
{
    /*  Key/certificate pair generated using the following command:
            openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 \
            -keyout localhost.key -passout pass:"test" -out localhost.crt \
            -subj "/CN=localhost"

        Diffie-Hellman parameter generated using the following command:
            openssl dhparam -dsaparam -out dh4096.pem 4096
    */

    wamp::SslContext ssl;

    ssl.setPasswordCallback(
        [](std::size_t, wamp::SslPasswordPurpose) {return "test";}).value();

    ssl.useCertificateChainFile("./certs/localhost.crt").value();

    ssl.usePrivateKeyFile("./certs/localhost.key",
                          wamp::SslFileFormat::pem).value();

#ifndef CPPWAMP_SSL_AUTO_DIFFIE_HELLMAN_AVAILABLE
    ssl.useTempDhFile("./certs/dh4096.pem").value();
#endif

    return ssl;
}

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

        auto tcpServerOptions =
            wamp::ServerOptions("tcp12345", wamp::TcpEndpoint{12345},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        auto tlsServerOptions =
            wamp::ServerOptions("tls23456",
                                wamp::TlsEndpoint{23456, &makeSslContext},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        logger({wamp::LogLevel::info, "CppWAMP example TLS router launched"});
        wamp::IoContext ioctx;

        wamp::Router router{ioctx, routerOptions};
        router.openRealm(realmOptions);
        router.openServer(tcpServerOptions);
        router.openServer(tlsServerOptions);

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
        logger({wamp::LogLevel::info, "CppWAMP example TLS router exit"});
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
