/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using Websocket transport.
//******************************************************************************

#include <cppwamp/session.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/websocketclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/callbacktimeclient.hpp"

//------------------------------------------------------------------------------
// Usage: cppwamp-example-wstimeclient [port [host [realm [target]]]] | help
// Use with cppwamp-example-wsrouter or cppwamp-example-httpserver.
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
    auto ws = wamp::WebsocketHost(host, port).withTarget(std::move(target))
                                             .withFormat(wamp::json);
    client->start(std::move(realm), std::move(ws));
    ioctx.run();
    return 0;
}
