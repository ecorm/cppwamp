CppWAMP
=======

C++11 client library for the [WAMP][wamp] protocol.

**Features**:

- Supports the WAMP _Basic Profile_
- Supports [some advanced WAMP profile features](#advanced)
- Roles: _Caller_, _Callee_, _Subscriber_, _Publisher_
- Transports: TCP and Unix domain raw sockets
- Serializations: JSON and MsgPack
- Provides both callback and coroutine-based asynchronous APIs
- Easy conversion between static and dynamic types
- RPC and pub/sub event handlers can have static argument types
- User-defined types can be registered and exchanged via RPC and pub-sub
- Header-only, but may also be optionally compiled
- Unit tested
- Permissive license (Boost)

**Dependencies**:

- [Boost.Asio][boost-asio] for raw socket transport (requires [Boost][boost] 1.74 or greater)
- (optional) [RapidJSON][rapidjson] for JSON serialization
- (optional) [msgpack-c][msgpack-c] for Msgpack serialization
- (optional) [Boost.Coroutine][boost-coroutine] and
  [Boost.Context][boost-context]
- (optional) [CMake][cmake] and the [Catch2][catch2] unit test framework

[wamp]: http://wamp-proto.org/
[boost-asio]: http://www.boost.org/doc/libs/release/doc/html/boost_asio.html
[boost]: http://boost.org
[rapidjson]: https://github.com/Tencent/rapidjson
[msgpack-c]: https://github.com/msgpack/msgpack-c
[boost-coroutine]: http://www.boost.org/doc/libs/release/libs/coroutine/doc/html/index.html
[boost-context]: http://www.boost.org/doc/libs/release/libs/context/doc/html/index.html
[cmake]: http://www.cmake.org/
[catch2]: https://github.com/catchorg/Catch2


Documentation
-------------

- [Tutorial](http://ecorm.github.io/cppwamp/_tutorial.html)
- [Reference Documentation](http://ecorm.github.io/cppwamp)
- [Release Notes](./CHANGELOG.md)


Tested Platforms
----------------

This library has been tested with:

- GCC x86_64-linux-gnu, versions 7.5 and 10.3
- Clang x86_64-pc-linux-gnu, versions 6.0.1 and 12.0,

<a name="advanced"></a>Supported Advanced Profile Features
----------------------------------------------------------

- General: agent identification, feature announcement
- _Callee_: `call_canceling`, `caller_identification`, `call_trustlevels`, `pattern_based_registration`, `progressive_call_results`
- _Caller_: `call_canceling`, `call_timeout`, `caller_identification`
- _Publisher_: `publisher_exclusion`, `publisher_identification`, `subscriber_blackwhite_listing`
- _Subscriber_: `pattern_based_subscription`, `publication_trustlevels`, `publisher_identification`


Roadmap
-------

### v1.0

- Adopt the [jsoncons](https://github.com/danielaparker/jsoncons) library's cursor interface to implement the `wamp::Json` and `wamp::Msgpack` codecs, while retaining `wamp::Variant` as the document type.
- Make `wamp::Session` more thread-safe.
- Remove `wamp::CoroSession` and make it so that `wamp::Session` can accept any completion token (`yield`, `use_future`, etc) supported by Boost.Asio.

### v1.1

- Add embedded router functionality

### v1.2 (maybe)

- Add websocket support via Boost.Beast

### v2.0 (maybe)

- Migrate from jsoncons to another serialization library currently in development, which features the ability to skip an intermediary variant type and directly encode/decode the network data into C++ data types.


Questions, Discussions, and Issues
----------------------------------

For general questions and discussions regarding CppWAMP, please use the
project's GitHub [discussions page][discussions].

For reporting bugs or for suggesting enhancements, please use the GitHub
[issue tracker][issues].

[discussions]: https://github.com/ecorm/cppwamp/discussions
[issues]: https://github.com/ecorm/cppwamp/issues


Usage Examples Using Coroutines
---------------------------------

_For a more comprehensive overview, check out the [Tutorials](https://github.com/ecorm/cppwamp/wiki/Tutorial) in the wiki._

### Establishing a WAMP session
```c++
wamp::AsioContext ioctx;
boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
{
    // Specify a TCP transport and JSON serialization
    auto tcp = wamp::connector<wamp::Json>(
        ioctx, wamp::TcpHost("localhost", 8001));
    auto session = wamp::CoroSession<>::create(ioctx, tcp);
    session->connect(yield);
    auto sessionInfo = session->join(wamp::Realm("myrealm"), yield);
    std::cout << "Client joined. Session ID = "
              << sessionInfo.id() << "\n";
    // etc.
    });
ioctx.run();
```

### Registering a remote procedure
```c++
boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
{
    :::
    session->enroll(wamp::Procedure("add"),
                    wamp::basicRpc<int, int, int>(
                        [](int n, int m) -> int {return n+m;}),
                    yield);
    :::
});
```

### Calling a remote procedure
```c++
auto result = session->call(wamp::Rpc("add").withArgs(2, 2), yield);
std::cout << "2 + 2 is " << result[0].to<int>() << "\n";
```

### Subscribing to a topic
```c++
void sensorSampled(float value)
{
    std::cout << "Sensor sampled, value = " << value << "\n";
}

boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
{
    :::
    session->subscribe(wamp::Topic("sensorSampled"),
                       wamp::basicEvent<float>(&sensorSampled),
                       yield);
    :::
});
```

### Publishing an event
```c++
float value = std::rand() % 10;
session->publish(wamp::Pub("sensorSampled").withArgs(value));
```


Usage Examples Using Asynchronous Callbacks
-------------------------------------------

### Establishing a WAMP session
```c++
class App : public std::enable_shared_from_this<App>
{ 
public:
    void start(wamp::AnyExecutor executor)
    {
        // Specify a TCP transport and JSON serialization
        auto tcp = wamp::connector<wamp::Json>(
            executor, wamp::TcpHost("localhost", 8001));

        session_ = wamp::Session::create(executor, tcp);

        // `self` is used to ensure the App instance still exists
        // when the callback is invoked.
        auto self = shared_from_this();

        // Perform the connection, then chain to the next operation.
        session_->connect(
            [this, self](wamp::AsyncResult<size_t> result)
            {
                // 'result' contains the index of the connector used to
                // establish the connection, or an error
                result.get(); // Throws if result contains an error
                onConnect();
            });
    }

private:
    void onConnect()
    {
        auto self = shared_from_this();
        session_->join(
            wamp::Realm("myrealm"),
            [this, self](wamp::AsyncResult<wamp::SessionInfo> info)
            {
                onJoin(info.get());
            });
    }

    void onJoin(wamp::SessionInfo info)
    {
        std::cout << "Client joined. Session ID = "
                  << info.id() << "\n";
        // etc...
    }

    wamp::Session::Ptr session_;
};

int main()
{
    wamp::AsioContext ioctx;
    App app;
    app.start(ioctx.get_executor());
    ioctx.run();
}

```

### Registering a remote procedure
```c++
class App : public std::shared_from_this<App>
{ 
public:
    void start(wamp::AnyExecutor executor) {/* As above */}

private:
    static int add(int n, int m) {return n + m;}

    void onConnect() {/* As above */}

    void onJoin(wamp::SessionInfo)
    {
        auto self = shared_from_this();
        session_->enroll(
            wamp::Procedure("add"),
            wamp::basicRpc<int, int, int>(&App::add),
            [this, self](AsyncResult<Registration> reg)
            {
                onRegistered(reg.get());
            });
    }

    void onRegistered(wamp::Registation reg)
    {
        // etc
    }

    wamp::Session::Ptr session_;
};

```

### Calling a remote procedure
```c++
class App : public std::shared_from_this<App>
{ 
public:
    void start(wamp::AnyExecutor executor) {/* As above */}

private:
    void onConnect() {/* As above */}

    void onJoin(wamp::SessionInfo info)
    {
        auto self = shared_from_this();
        session_->call(
            wamp::Rpc("add").withArgs(2, 2),
            [this, self](wamp::AsyncResult<wamp::Result> sum)
            {
                onAdd(sum.get());
            });
    }

    void onAdd(wamp::Result result)
    {
        std::cout << "2 + 2 is " << result[0].to<int>() << "\n";
    }

    wamp::Session::Ptr session_;
};
```

### Subscribing to a topic
```c++
class App : public std::shared_from_this<App>
{ 
public:
    void start(wamp::AnyExecutor executor) {/* As above */}

private:
    static void sensorSampled(float value)
    {
        std::cout << "Sensor sampled, value = " << value << "\n";
    }

    void onConnect() {/* As above */}

    void onJoin(wamp::SessionInfo info)
    {
        auto self = shared_from_this();
        session_->subscribe(
            wamp::Topic("sensorSampled"),
            wamp::basicEvent<float>(&App::sensorSampled),
            [this, self](wamp::AsyncResult<wamp::Subscription> sub)
            {
                onSubscribed(sub.get());
            });
    }

    void onSubscribed(wamp::Subscription sub)
    {
        std::cout << "Subscribed, subscription ID = " << sub.id() << "\n";
    }

    wamp::Session::Ptr session_;
};
```

### Publishing an event
```c++
float value = std::rand() % 10;
session_->publish(wamp::Pub("sensorSampled").withArgs(value));
```


Header-Only Installation
------------------------

CppWAMP supports header-only usage. You may simply add the include directories of CppWAMP and its dependencies into your project's include search path. On GCC/Clang, this can be done with the `-isystem` compiler flag. You'll also need to link to the necessary Boost libraries.

You may use CppWAMP's CMake scripts to fetch and build dependencies. The following commands will clone the CppWAMP repository, build the third-party dependencies, and install the headers and CMake package config:

```bash
git clone https://github.com/ecorm/cppwamp
cd cppwamp
cmake -DCPPWAMP_OPT_VENDORIZE -DCPPWAMP_OPT_HEADERS_ONLY -S . -B ./_build
cmake --build ./_build
cmake --install ./_build --prefix ./_stage/cppwamp
```

Two subdirectories will be generated as a result:

- `_build` will contain intermediary build files and may be deleted.
- `_stage` will contain third-party dependencies, as well as the CppWAMP headers and its CMake package config.

You may then use the following GCC/Clang compiler flags:
```
-isystem path/to/cppwamp/_stage/boost/include
-isystem path/to/cppwamp/_stage/cppwamp/include
-isystem path/to/cppwamp/_stage/msgpack/include
-isystem path/to/cppwamp/_stage/rapidjson/include
```

as well as the following GCC/Clang linker flags:
```
-Lpath/to/cppwamp/_stage/boost/lib
-lboost_coroutine -lboost_context -lboost_thread -lboost_system
```

Note that that only `-lboost_system` is necessary if you're not using the coroutine API.

You may omit the `-DCPPWAMP_OPT_VENDORIZE` option if you want to use the third-party libraries installed on your system. You may provide hints to their location via the following CMake configuration options:

- `-DBoost_ROOT=path/to/boost`
- `-DRapidJSON_ROOT=path/to/rapidjson`
- `-DMsgpack_ROOT=path/to/msgpack`


Compiling the library, tests, and examples
------------------------------------------

The steps are similar to the above _Header-Only Installation_, except that the `-DCPPWAMP_OPT_HEADERS_ONLY` option is omitted.

```bash
git clone https://github.com/ecorm/cppwamp
cd cppwamp
cmake -DCPPWAMP_OPT_VENDORIZE -S . -B ./_build
cmake --build ./_build
cmake --install ./_build --prefix ./_stage/cppwamp
```

The necessary compiler flags to use in your project are the same as the above _Header-Only Installation_, with the following extra needed linker flags:

```
-L path/to/cppwamp/_stage/cppwamp/lib
-lcppwamp-json
-lcppwamp-msgpack
-lcppwamp-core
```

where `-lcppwamp-json` or `-lcppwamp-msgpack` may be omitted if your project doesn't use that particular serialization.

Consult the root CMakeLists.txt file for a list of `CPPWAMP_OPT_<option>` cache variables that control what's included in the build.

Integrating CppWAMP into a CMake-based Project
----------------------------------------------

### With add_subdirectory

The following example CMakeLists.txt shows how to include CppWAMP via `add_subdirectory`:

```cmake
cmake_minimum_required (VERSION 3.12)
project(MyApp)

if(allow_cppwamp_to_download_and_build_dependencies)
    option(CPPWAMP_OPT_VENDORIZE "" ON)
else()
    option(CPPWAMP_OPT_VENDORIZE "" OFF)
    option(CPPWAMP_OPT_WITH_MSGPACK "" OFF) # Only JSON is needed for
                                            # this example
    # If the following are not set, CppWAMP will use the default
    # search paths of CMake's find_package() to find its dependencies.
    set(Boost_ROOT /path/to/boost)
    set(Msgpack_ROOT /path/to/msgpack)
endif()

add_subdirectory(cppwamp)
add_executable(myapp main.cpp)
target_link_libraries(myapp
    PRIVATE CppWAMP::json CppWAMP::coro-headers)
```

Any of the `CppWAMP::*` targets will automatically add the basic usage requirements of CppWAMP into your app's generated compiler/linker flags. The `CppWAMP::json` target, for example, will add the `libcppwamp-json` library, as well as the RapidJSON include search path.

Please consult CppWAMP's root CMakeLists.txt file for a complete list of targets that you may specify for your app's `target_link_libraries`.

### With FetchContent

You can use CMake's FetchContent to download CppWAMP and its dependencies at configuration time from within your project's CMakeLists.txt, as shown in the following example.

```cmake
cmake_minimum_required (VERSION 3.12)
project(MyApp)

if(allow_cppwamp_to_download_and_build_dependencies)
    option(CPPWAMP_OPT_VENDORIZE "" ON)
else()
    option(CPPWAMP_OPT_VENDORIZE "" OFF)
    option(CPPWAMP_OPT_WITH_MSGPACK "" OFF) # Only JSON is needed for
                                            # this example
    # If the following are not set, CppWAMP will use the default
    # search paths of CMake's find_package() to find its dependencies.
    set(Boost_ROOT /path/to/boost)
    set(RapidJSON_ROOT /path/to/rapidjson)
endif()

include(FetchContent)
FetchContent_Declare(
    cppwamp
    GIT_REPOSITORY https://github.com/ecorm/cppwamp
)
FetchContent_MakeAvailable(cppwamp)

add_executable(myapp main.cpp)

target_link_libraries(myapp
                      PRIVATE CppWAMP::json CppWAMP::coro-headers)
```

### With find_package

If you do not wish to embed CppWAMP as a subdirectory of your project, you may use `find_package` instead:

```cmake
cmake_minimum_required (VERSION 3.12)
project(MyApp)

# If the following are not set, CppWAMP will use the default
# search paths of CMake's find_package() to find its dependencies.
set(Boost_ROOT /path/to/boost)
set(RapidJSON_ROOT /path/to/rapidjson)
set(Msgpack_ROOT /path/to/msgpack)

find_package(CppWAMP
             REQUIRED coro-headers json
             CONFIG
             PATHS /path/to/cppwamp_installation
             NO_DEFAULT_PATH)

add_executable(myapp main.cpp)
target_link_libraries(myapp
    PRIVATE CppWAMP::json CppWAMP::coro-headers)
```

This method requires that CppWAMP be previously built (either as header-only, or compiled) and installed so that its CMake package config (i.e. `CppWAMPConfig.cmake`) is generated. This can either be done outside of your project or via your project's CMake scripts (for example by using `ExternalProject_add` or `FetchContent`).


License
-------

```
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

* * *
_Copyright © Butterfly Energy Systems, 2014-2022_
