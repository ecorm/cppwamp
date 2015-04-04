<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Remote Procedure Calls
======================

Calling RPCs
------------

The `call` operation is used to call remote procedures. There are two `call` overloads: one that takes an `Args` bungle, and another that doesn't. The latter overload is used when the remote procedure does not expect any arguments. For either overload, `call` returns an `Args` object, which contains the results returned by the remote procedure.

```c++
boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
{
    auto client = CoroClient<>::create({tcpJson, tcpPack});
    client->connect(yield);
    client->join("somerealm", yield);

    // Call a remote procedure that takes no arguments
    Args result = client->call("getElapsed", yield);
    Int elapsed = 0;
    result.to(elapsed); // Will throw error::Conversion if conversion fails
    std::cout << "The elapsed number of seconds is " << elapsed << "\n";

    // Call a remote procedure that takes two positional arguments
    auto sum = client->call("add", {12, 34}, yield);
    std::cout << "12 + 34 is " << sum[0] << "\n";

    // Call a remote procedure that takes a single keyword argument and does
    // not return any result.
    client->call("setProperty", {withPairs, { {"name", "John"} }}, yield);
});
```

Call Slots
----------

CppWamp allows you to register callback functions that will be executed whenever a procedure with a certain name is invoked remotely by a dealer. Such callback functions are named _call slots_. Call slots can be any callable entity that can be stored into a [`std::function`][stdfunction]:
- free functions,
- member functions (via [`std::bind`][stdbind]),
- function objects, or,
- lambdas.

[stdfunction]: http://en.cppreference.com/w/cpp/utility/functional/function
[stdbind]: http://en.cppreference.com/w/cpp/utility/functional/bind

Registering Parameterless (_void_) Procedures
---------------------------------------------

A _void_ procedure is an RPC that does not expect any arguments. A call slot handling such a procedure must have the following signature:
```c++
void procedure(wamp::Invocation)
```
where `wamp::Invocation` is an object that provides the means for returning a `YIELD` or `ERROR` result back to the remote caller.

`enroll<void>()` is used to register a parameterless RPC. `enroll` returns a reference-counting `Registration` handle. Every time a copy of this handle is made, the reference count increases. Conversely, every time a bound
`Registration` handle is destroyed, the reference count decreases. When the reference count reaches zero, the RPC is automatically unregistered. This reference counting scheme is provided to help automate the management of RPC
registrations using RAII techniques.

Note that RPCs can be manually unregistered via `Registration::unregister`. Duplicate unregistrations are safely ignored by the library.

The following example demonstrates the registering and automatic unregistering of a remote procedure call:

```c++
// The call slot to be used for the "getDiceRoll" RPC
void getDiceRoll(Invocation inv)
{
    int num = std::rand() % 6;
    inv.yield({num}); // Send result back to the remote caller
}

boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
{
    auto callee = CoroClient<>::create(tcp);
    auto caller = CoroClient<>::create(tcp);
    // Make caller and callee join the same WAMP realm...

    // Start a limited scope to demonstrate automatic unregistration
    {
        // Register the RPC
        Registration reg = callee->enroll<void>(
            "getDiceRoll", // The name of the procedure to register
            &getDiceRoll,  // The call slot to invoke for this RPC
            yield
        );

        // Invoke the RPC
        auto result = caller->call("getDiceRoll", yield);
        std::cout << "Dice roll is " << result[0] << "\n";

        // 'reg' handle goes out of scope here
    }

    // When the registration handle reference count reaches zero, it
    // automatically unregisters the RPC.

    try
    {
        // The following remote procedure call will fail, because 'getDiceRoll'
        // is no longer registered.
        auto result = caller->call("getDiceRoll", yield);
    }
    catch (const error::Wamp& e)
    {
        if (e.code() == WampErrc::noSuchProcedure)
            std::cerr << "getDiceRoll RPC no longer exists!\n";
    }
});
```

Registering Statically-Typed Procedures
---------------------------------------

Certain remote procedures may expect a fixed number of statically-typed arguments. For these situations, the following form of the `enroll` method is used:
```c++
enroll<StaticTypeList...>("procedure", slot, yield);
```
where:
- _StaticTypeList..._ is a parameter pack of static types
- _slot_ has the signature:
   ```c++
   void procedure(wamp::Invocation, StaticTypeList...)
   ```

Example:
```c++
void addToCart(Invocation inv, std::string item, int cost)
{
    // Process invocation...
    inv.yield(result); // Send result to peer
}

// Within coroutine
client->enroll<std::string, int>("addToCart", &addToCart, yield);
//             ^^^^^^^^^^^^^^^^
//             Note that these types match the ones in the `addToCart` signature
```

Registering Dynamically-Typed Procedures
----------------------------------------

For some remote procedures, it may not be possible to know the number and types of the arguments passed in by the remote callee. For these situations, the following form of the `enroll` method is used:
```c++
enroll<Args>("procedure", slot, yield);
```
where:
- `Args` is a bundle of dynamic arguments (positional and/or keyword)
- _slot_ has the signature:
   ```c++
   void procedure(wamp::Invocation, Args)
   ```

Example:
```c++
void addToCart(Invocation inv, Args args)
{
    std::string item;
    int cost;
    args.to(item, cost); // throws if conversions fail
    // Process invocation...
    inv.yield(result); // Send result to peer
}

// Within coroutine
client->enroll<Args>("addToCart", &addToCart, yield);
```

Returning an ERROR Response to a Callee
---------------------------------------
`Invocation::fail` can be used to return an `ERROR` response to a callee. This would happen, for example, when invalid arguments are passed to a dynamically typed remote procedure:

```c++
void addToCart(Invocation inv, Args args)
{
    if (args.list.size() != 2)
    {
        inv.fail("wamp.error.invalid_argument");
    }
    else
    {
        std::string item;
        int cost;
        args.to(item, cost);
        // Process invocation...
        inv.yield(result); // Send result to peer
    }
}
```
