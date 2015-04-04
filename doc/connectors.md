<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Connectors
==========

`Connector` is the abstract base class for objects that can establish client transport endpoints.

The library currently supports two types of connectors:

Connector      | Declared in                  | For Transport
-------------- | ---------------------------- | -----------
`TcpConnector` | `<cppwamp/tcpconnector.hpp>` | TCP raw socket
`UdsConnector` | `<cppwamp/udsconnector.hpp>` | Unix Domain Socket

The above connectors support raw socket handshaking, introduced in [version e2c4e57][e2c4e57] of the advanced WAMP specification.

[e2c4e57]: https://github.com/tavendo/WAMP/commit/e2c4e5775d89fa6d991eb2e138e2f42ca2469fa8

For WAMP routers that do not yet support handshaking, alternate connectors are available under the `wamp::legacy` namespace:

Connector              | Declared in                        | For Transport
---------------------- | ---------------------------------- | -------------
`legacy::TcpConnector` | `<cppwamp/legacytcpconnector.hpp>` | Legacy TCP raw socket
`legacy::UdsConnector` | `<cppwamp/legacyudsconnector.hpp>` | Legacy Unix Domain Socket

When creating a connector, you must specify which serialization (aka codec) to use for encoding WAMP messages. The following serializers are currently supported:

Serializer | Codec ID              | Declared in
---------- | --------------------- | -----------
JSON       | `wamp::Json::id()`    | `<cppwamp/json.hpp>`
Msgpack    | `wamp::Msgpack::id()` | `<cppwamp/msgpack.hpp>`

Connectors must be created on the heap using the `create` factory method, as shown in the examples below. `create` will return a `std::shared_ptr` to the newly created connector.

After you have created one or more connectors, you pass them to the client API. The client interface will then use these connectors while establishing a transport connection to the router.

Creating a TCP Raw Socket Connector
-----------------------------------

```c++
#include <cppwamp/client.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/tcpconnector.hpp>
using namespace wamp;

IoService iosvc; // IoService is an alias to boost::asio::io_service
auto tcp = TcpConnector::create(
    iosvc,          // The I/O service to use for asynchronous operations
    "127.0.0.1",    // Host address
    8001,           // Port number (could also be a service name string)
    CodecId::json,  // Use JSON for message serialization
    RawsockMaxLength::kB_64 // Limit received messages to 64 kilobytes
    );

// Create a `Client` object which will later use the above connector when
// establishing a transport connection.
auto client = Client::create(tcp);
```

Creating a Unix Domain Socket Connector
---------------------------------------

```c++
#include <cppwamp/client.hpp>
#include <cppwamp/udsconnector.hpp>
#include <cppwamp/msgpack.hpp>
using namespace wamp;

IoService iosvc; // IoService is an alias to boost::asio::io_service
auto uds = UdsConnector::create(
    iosvc,              // The I/O service to use for asynchronous operations
    "path/to/uds",      // Path name of the Unix domain socket
    CodecId::msgpack,   // Use msgpack for message serialization
    RawsockMaxLength::MB_1 // Limit received messages to 1 megabyte
    );

// Create a `Client` object which will later use the above connector when
// establishing a transport connection.
auto client = Client::create(uds);
```

Combining Connectors
--------------------

More than one `Connector` can be passed to a client object. In such cases, the client will successively attempt to establish a transport connection with each of the connectors until one succeeds. This allows you to specify "fallback"
transports if the primary one fails to establish.

To achieve this, you must pass a `std::vector` of `Connector` shared pointers while creating client objects. The following example uses two `TcpConnector` objects, each specifying a different serializer:
```c++
#include <cppwamp/client.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/tcpconnector.hpp>
using namespace wamp;

IoService iosvc;
auto tcpJson = TcpConnector::create(iosvc, "localhost", 8001, CodecId::json);
auto tcpPack = TcpConnector::create(iosvc, "localhost", 8001, CodecId::msgpack);

// Create a `Client` object which will later use both of the above connectors
// (in succession) when establishing a transport connection.
auto client = Client::create({tcpJson, tcpPack});
```
