/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#define BOOST_THREAD_THROW_IF_PRECONDITION_NOT_SATISFIED

#include <iostream>
#include <thread>
#include <cppwamp/asioexecutor.hpp>
#include <cppwamp/futusession.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/unpacker.hpp>

#ifdef CPPWAMP_USE_LEGACY_CONNECTORS
#include <cppwamp/legacytcpconnector.hpp>
using wamp::legacy::TcpConnector;
#else
#include <cppwamp/tcpconnector.hpp>
using wamp::TcpConnector;
#endif

const std::string realm = "cppwamp.demo.futucalc";
const std::string address = "localhost";
const short port = 12345;

//------------------------------------------------------------------------------
wamp::Outcome add(wamp::Invocation, int a, int b)
{
    return {a+b};
}

//------------------------------------------------------------------------------
// This program demonstrates a remote calculator service using the future-based
// API. Both clients are executed within this same program for demonstration
// purposes. Normally, they would each be run as separate programs.
//------------------------------------------------------------------------------
int main()
{
    using namespace wamp;

    AsioService iosvc;

    // All continuations shall be run via this executor, which posts
    // asynchronous tasks to the IO service.
    AsioExecutor exec(iosvc);

    auto tcp = TcpConnector::create(iosvc, "localhost", 12345,
                                    wamp::Json::id());

    auto calc = FutuSession<>::create(tcp);
    auto client = FutuSession<>::create(tcp);

    using boost::future;

    // Futures cannot be returned from continuations with executors, due to this
    // known Boost.Thread bug: https://svn.boost.org/trac/boost/ticket/11192

    // Make the calculator service connect, join, and register an RPC.
    bool calcReady = false;
    future<void> f2, f3;
    auto f1 = calc->connect()
        .then(exec, [&](future<size_t>)
        {
            f2 = calc->join(wamp::Realm(realm))

        .then(exec, [&](future<SessionInfo> info)
        {
            std::cout << "Callee session ID = " << info.get().id() << "\n";
            f3 = calc->enroll(Procedure("add"), unpackedRpc<int, int>(&add))

        .then(exec, [&](future<Registration> reg)
        {
            std::cout << "Registration ID = " << reg.get().id() << "\n";
            calcReady = true;
        }); }); });


    // Make the client connect and join.
    bool clientReady = false;
    future<void> f5;
    auto f4 = client->connect()
        .then(exec, [&](future<size_t>)
        {
            f5 = client->join(wamp::Realm(realm))

        .then(exec, [&](future<SessionInfo> info)
        {
            std::cout << "Caller session ID = " << info.get().id() << "\n";
            clientReady = true;
        }); });

    // Run the IO service until both caller and callee are ready
    exec.reschedule_until([&] {return clientReady && calcReady;});

    // Make the client call a remote procedure.
    auto f6 = client->call(Rpc("add").withArgs({12, 34})).then(exec,
        [&](future<Result> result)
        {
            std::cout << "12 + 34 is " << result.get()[0] << "\n";
            exec.close(); // Stop the IO service
        });

    // Run the IO service until it is stopped.
    exec.loop();

    return 0;
}
