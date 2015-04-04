<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Asynchronous Client API
=======================

The `wamp::Client` class provides an asynchronous API that should be familiar
to those who have used Boost.Asio. Wherever `CoroClient` expects an yield
context parameter, `Client` instead expects a callable entity that is invoked
when the asynchronous operation is completed.

For asynchronous operations that can fail, `Client` expects a handler taking an
`AsyncResult` as a parameter. `AsyncResult` makes it impossible for handlers to
ignore error conditions when accessing the result of an asynchronous operation.

The following example shows how to establish a connection using the asynchronous
`Client` API:

```c++
#include <cppwamp/client.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/tcpconnector.hpp>
using namespace wamp;

IoService iosvc;
auto tcp = TcpConnector::create(iosvc, "localhost", 8001, CodecId::json);

auto client = Client::create(tcp);

// Asynchronously connect to the router, using a lambda function for the handler
client->connect( [](AsyncResult<size_t> result)
{
    // Obtain the result of the connect operation, which is the index of
    // the connector used to establish the transport connection.
    // AsyncResult::get throws an error::Wamp exception if the operation failed.
    size_t index = result.get();
});

iosvc.run();
```
Instead of letting `AsyncResult::get` throw an exception upon failure,
`AsyncResult::operator bool` and `AsyncResult::errorCode` can be used to check
the error status of an asynchronous operation:

```c++
client->connect( [](AsyncResult<size_t> result)
{
    if (result)
    {
        size_t index = result.get();
        // Continue as normal...
    }
    else
    {
        std::cerr << "Failure connecting: " << result.errorCode() << "\n";
    }
});
```

The following example shows how to call member functions within asynchronous
handlers, and how to chain one asynchronous operation after another:

```c++
class App
{
public:
    explicit App(wamp::Connector::Ptr connector)
        : client_(wamp::Client::create(connector))
    {}

    void start()
    {
        client_->connect( [this](wamp::AsyncHandler<size_t> result)
        {
            result.get(); // throws if connection failed
            this->join();
        });
    }

private:
    void join()
    {
        client_->join("somerealm",
            [this](wamp::AsyncHandler<wamp::SessionId> result)
            {
                sessionId_ = result.get();
                // Continue with other asynchronous operations...
            }
        );
    }

    wamp::Client::Ptr client_;
    wamp::SessionId sessionId_;
};
```

With this asynchronous style of programming, it is not immediately obvious
how the program control flows. The coroutine-based API is therefore useful in
implementing asynchronous logic in a more synchronous, sequential manner.
