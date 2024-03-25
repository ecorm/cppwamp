/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using Websocket Secure transport.
//******************************************************************************

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <cppwamp/session.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/wssclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/callbacktimeclient.hpp"
#include "../common/sslclient.hpp"

//------------------------------------------------------------------------------
// Usage: cppwamp-example-wstimeclient [port [host [realm]]] | help
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ArgsParser args{{{"port",   "23456"},
                     {"host",   "localhost"},
                     {"realm",  "cppwamp.examples"},
                     {"target", "/time"}}};

    unsigned short port = 0;
    std::string host, realm, target;
    if (!args.parse(argc, argv, port, host, realm, target))
        return 0;

    wamp::IoContext ioctx;
    auto client = TimeClient::create(ioctx.get_executor());

    auto verif = wamp::SslVerifyOptions{}.withMode(wamp::SslVerifyMode::peer())
                     .withCallback(&verifySslCertificate);

    auto wss = wamp::WssHost(host, port, &makeClientSslContext)
                   .withTarget(std::move(target))
                   .withSslVerifyOptions(std::move(verif))
                   .withFormat(wamp::json);

    client->start(std::move(realm), std::move(wss));
    ioctx.run();
    return 0;
}
