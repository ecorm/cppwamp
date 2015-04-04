<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Error Handling with Couroutines
===============================

Exceptions
----------

Whenever a `CoroClient` operation fails, it throws an `error::Wamp` exception. `error::Wamp` derives from [`std::system_error`][system_error], thus it contains a [`std::error_code`][error_code] that represents the cause of the error.

[system_error]: http://en.cppreference.com/w/cpp/error/system_error
[error_code]: http://en.cppreference.com/w/cpp/error/error_code

```c++
IoService iosvc;
auto tcp = TcpConnector::create(iosvc, "localhost", 8001, CodecId::json);

boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
{
    auto client = CoroClient<>::create(tcp);
    try
    {
        client->connect(yield);
        SessionId sid = client->join("somerealm", yield);
        // etc.
    }
    catch (const error::Wamp& e)
    {
        // Print a message describing the error
        std::cerr << e.what() << "\n";

        // Obtain the std::error_code associated with the exception
        auto ec = e.code();
        if (ec == WampErrc::noSuchRealm)
            std::cerr << "The realm doesn't exist\n";
    }
});

iosvc.run();

```

Error Codes
-----------

An error code can either belong to [`std::generic_category`][generic_category], or to one of the error categories defined by the library in `<cppwamp/error.hpp>`:

Error category            | Values                | Used for reporting
------------------------- | ----------------------| ------------------
`wamp::WampCategory`      | `wamp::WampErrc`      | WAMP layer errors
`wamp::ProtocolCategory`  | `wamp::ProtocolErrc`  | invalid WAMP messages
`wamp::TransportCategory` | `wamp::TransportErrc` | general transport layer errors
`wamp::RawsockCategory`   | `wamp::RawsockErrc`   | raw socket transport errors
`std::generic_category`   | `std::errc`           | OS-level socket errors

[generic_category]: http://en.cppreference.com/w/cpp/error/generic_category

CoroErrcClient API
------------------

If you prefer that operations return error codes instead of throwing exceptions, you may use the `CoroErrcClient` API instead. `CoroErrcClient` is the same as `CoroClient`, except that operations which can fail take an additional
`std::error_code` parameter by reference.

```c++
boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
{
    auto client = CoroErrcClient<>::create(tcp);
    std::error_code ec;
    client->connect(yield, ec);
    if (ec)
    {
        std::cerr << "Error: " << ec << "\n";
    }
    // etc.
});

```

Note that `CoroErrcClient` will still throw `error::Logic` exceptions whenever preconditions are not met. Preconditions for API functions are listed in the reference documentation.
