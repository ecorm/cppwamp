/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using TLS transport.
//******************************************************************************

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <cppwamp/session.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tlsclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/callbacktimeclient.hpp"
#include "../common/sslclient.hpp"

//------------------------------------------------------------------------------
// Usage: cppwamp-example-wstimeclient [port [host [realm]]] | help
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // TODO: Capture SIGINT/SIGTERM signal and perform orderly TLS shutdown

    ArgsParser args{{{"port", "23456"},
                     {"host", "localhost"},
                     {"realm", "cppwamp.examples"}}};

    unsigned short port = 0;
    std::string host;
    std::string realm;
    if (!args.parse(argc, argv, port, host, realm))
        return 0;

    wamp::IoContext ioctx;
    auto client = TimeClient::create(ioctx.get_executor());

    auto verif = wamp::SslVerifyOptions{}.withMode(wamp::SslVerifyMode::peer())
                     .withCallback(&verifySslCertificate);

    auto tls = wamp::TlsHost(host, port, &makeClientSslContext)
                   .withSslVerifyOptions(std::move(verif))
                   .withFormat(wamp::json);

    client->start(std::move(realm), std::move(tls));
    ioctx.run();
    return 0;
}
