<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Session Management using Coroutines
===================================

Because it makes things easier to demonstrate, the following examples will use
the coroutine-based client API. The asynchronous client API will be covered
later in another topic.

Creating the Client Interface
-----------------------------

Once one or more Connector objects have been created, they are passed to the
`create` factory method of the client API. `create` then returns a
`std::shared_ptr` to the newly created client interface.

```c++
#include <cppwamp/coroclient.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/tcpconnector.hpp>
using namespace wamp;

IoService iosvc;
auto tcpJson = TcpConnector::create(iosvc, "localhost", 8001, CodecId::json);
auto tcpPack = TcpConnector::create(iosvc, "localhost", 8001, CodecId::msgpack);

auto client = CoroClient<>::create({tcpJson, tcpPack});
```

Connecting and Joining
----------------------

After the client interface object is created, the `connect` operation is used to
establish a transport connection to a WAMP router. The `join` operation is then
used to join a WAMP realm on the router.

All "blocking" operations of `CoroClient` must be executed within the context
of a coroutine. Coroutines are initiated via `boost::asio::spawn`.
For any `CoroClient` operation that needs to "block" until completion, a _yield
context_ must be provided. The following example shows how to spawn a coroutine,
and how a yield context is passed to the `connect` and `join` operations.

```c++
IoService iosvc;
auto tcpJson = TcpConnector::create(iosvc, "localhost", 8001, CodecId::json);
auto tcpPack = TcpConnector::create(iosvc, "localhost", 8001, CodecId::msgpack);

boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
{
    auto client = CoroClient<>::create({tcpJson, tcpPack});
    client->connect(yield);
    SessionId sid = client->join("somerealm", yield);
    // etc.
});

iosvc.run();

```

Note that while a coroutine operation is suspended, it yields control so that
other asynchronous operations can be executed via the I/O service.

Leaving and Disconnecting
-------------------------

To gracefully end a WAMP session, the `leave` and `disconnect` operations are
used:

```c++
boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
{
    auto client = CoroClient<>::create({tcpJson, tcpPack});
    client->connect(yield);
    SessionId sid = client->join("somerealm", yield);

    while (!finished)
    {
        // Interact with the WAMP session
    }

    std::string reason = client->leave(yield); // returns the Reason URI from
                                               // the router's GOODBYE message
    client->disconnect(); // non-blocking
});
```

To abruptly end a WAMP session, you can skip the `leave` operation and just
`disconnect`. This may be useful when handling error conditions. Alternatively,
you can let the client `shared_ptr` reference drop to zero, and the client
will abruptly terminate the connection in its destructor:

```c++
boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
{
    auto client = CoroClient<>::create({tcpJson, tcpPack});
    client->connect(yield);
    SessionId sid = client->join("somerealm", yield);

    while (!finished)
    {
        // Interact with the WAMP session
    }

    // The client reference count will drop to zero when this lambda function
    // exits. The client destructor will then terminate the connection.
});
```
