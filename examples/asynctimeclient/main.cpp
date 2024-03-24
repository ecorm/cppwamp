/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using callback handler functions.
//******************************************************************************

#include <cppwamp/session.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/callbacktimeclient.hpp"

//------------------------------------------------------------------------------
// Usage: cppwamp-example-asynctimeclient [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-asynctimeservice.
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ArgsParser args{{{"port", "12345"},
                     {"host", "localhost"},
                     {"realm", "cppwamp.examples"}}};

    std::string port, host, realm;
    if (!args.parse(argc, argv, port, host, realm))
        return 0;

    wamp::IoContext ioctx;
    auto client = TimeClient::create(ioctx.get_executor());
    client->start(realm, wamp::TcpHost(host, port).withFormat(wamp::json));
    ioctx.run();
    return 0;
}
