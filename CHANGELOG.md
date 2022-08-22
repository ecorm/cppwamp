v0.11.0
=======
Polymorphic codecs and transports.

- Added `ConnectionWish` and `ConnectionWishList` which should now be used
  in place of the old `Connection` and `ConnectionList` classes.
- Passing `ConnectionWish` and `ConnectionWishList` via `Session::connect` is
  now preferred over passing the legacy `Connector` instances via
  `Session::create`.
- `TcpHost` and `UdsPath` now have `withFormat` methods which generate a
  `ConnectionWish` that can be passed to `Session`.
- Relaxed `Session::state` preconditions for `Session`'s `unsubscribe` and
  `unregister` operations taking completion handlers. If the session state
  does not allow the transmission of `UNSUBSCRIBE` and `UNREGISTER` WAMP
  messages, a warning is emitted instead of an error being emitted via the
  completion handler.
- `Session::yield` operations while not established will emit a warning instead
  of failing.
- Added `Session::ongoingCall` for progressive call results, which
  automatically applies `rpc.withProgessiveResults(true)`.
- Handlers registered via `Session`'s `setWarningHandler`, `setTraceHandler`,
  and `setStateChangeHandler` will no longer be fired after
  `Session::tenminate` is called and before `Session::connect` is called.

Implementation improvements:

- Codecs are now specializations of `SinkEncoder` and `SourceDecoder`.
- Simplified codec tags to only provide their numeric ID.
- Added `AnyCodec` polymorphic wrapper for codecs.
- Added `Transporting` interface class which replaces the old Transport
  type requirement.
- `internal::Client` is now non-templated and is retained by `Session` during
  the latter's lifetime.
- `Session::connect` logic has been moved to `internal::Client`.
- Renamed `internal::AsioTransport` to `internal::RawsockTransport` and
  simplified its previously convoluted design.
- `internal::RawsockConnector` and `internal::RawsockTransport` now use
  policy classes instead of polymorphism to alter their behavior for tests.
- Tidying of transport tests.

### Beaking Changes

- `Session::call` can no longer be used for progessive call results, use
  `Session::ongoingCall` instead.
- The `Session` destructor now automatically invokes `Session::disconnect`
  instead of `Session::terminate`, to better emulate the cancellation behavior
  of Asio sockets being destroyed.
  
### Migration Guide

- Replace `connection<TCodec>(TcpHost)` with `TcpHost::withFormat`.
  E.g.: `wamp::TcpHost{"localhost", 12345}.withFormat(wamp::json)`
- Replace `Session::call` with `Session::ongoingCall` when progressive results
  are desired.
- Manually call `Session::terminate` before a `Session` is destroyed if you
  must suppress the execution of pending completion handlers.


v0.10.0
=======
Asio completion token support and thread-safe Session operations.

- All asynchronous operations in Session now accept a generic
  [completion token](https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/model/completion_tokens.html),
  which can either be a callback function, a `yield_context`, `use_awaitable`,
  or `use_future`.
- C++20 coroutines now supported by Session.
- Added examples using Asio stackless coroutines, C++20 coroutines, and
  std::future.
- Migrated from `AsyncResult` to new `ErrorOr` class which better emulates
  the proposed `std::expected`.
- `Session`'s asynchonous operations now return an `ErrorOr` result when passed
  a `yield_context` as the completion token, and will not throw if there was
  a runtime error. `ErrorOr::value` must be called to obtain the operation's
  actual result (or throw an exception if there was an error).
- Added `Session::strand` so that users may serialize access to the `Session`
  when using a thread pool.
- Added `Session` overloads with the `ThreadSafe` tag type which can be called
  concurrently by multiple threads. These overloads will be automatically
  dispatch operations via the `Session`'s execution strand.
- Added the `SessionErrc::invalidState` enumerator which is now used to report
  errors when attempting to perform `Session` operations during an invalid
  session state.
- Renamed `AnyExecutor` to `AnyIoExecutor` which aliases
  `boost::asio::any_io_executor`. AnyExecutor is now deprecated.
- Added `AnyReusableHandler` which type-erases a copyable multi-shot handler,
  while storing the executor to which it is possibly bound.
- Added `AnyCompletionHandler` which is a Boost-ified version of the prototype
  [asio::any_completion_handler]
  (https://github.com/chriskohlhoff/asio/issues/1100).
- Added `AnyCompletionExecutor` which is a Boost-ified version of the prototype
  `asio::any_completion_executor`.
- Session and transports now extract a
  [strand](https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/strands.html)
  from the `Connector` passed by the user.
- Moved corounpacker implementation to header directory root.
- Added `Realm::captureAbort`.
- Made config.hpp a public header.
- Added DecodingErrc and DecodingCategory for deserialization errors
  not covered by jsoncons.
- `Session`'s `setWarningHandler`, `setTraceHandler`, `setStateChangeHandler`,
  and `setChallengeHandler` now take effect immediately even when connected.
- `Session`'s handlers for `setWarningHandler`, `setTraceHandler`,
  `setStateChangeHandler`, and `setChallengeHandler` are now executed via
  `Session::userExecutor` by default.
- Boost.Asio [cancellation slot]
  (https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/cancellation.html)
  support for `Session::call`.
- Added `Rpc::withCancelMode` which specifies the cancel mode to use when
  triggered by Asio cancellation slots.
- Added `withArgsTuple`, `convertToTuple` and `moveToTuple` to Payload class.
- Added the `Deferment` tag type (with `deferment` constexpr variable`) to
  more conveniently return a deferred `Outcome` from an RPC handler.
- Renamed `Cancellation` to `CallCancellation`. `Cancellation` is now a
  deprecated alias to ``CallCancellation`.
- Renamed `CancelMode` to `CallCancelMode`. `CancelMode` is now a deprecated
  alias to `CallCancelMode`.
- Renamed `Basic[Coro]<Event|Invocation>Unpacker` to
  `Simple[Coro]<Event|Invocation>Unpacker`, with the former kept as deprecated
  aliases.
- Renamed `basic[Coro]<Event|Rpc>` to `simple[Coro]<Event|Rpc>`, with the
  former kept as deprecated aliases.
- Renamed the CppWAMP::coro-headers CMake target to CppWAMP::coro-usage,
  leaving the former as an alias.
- Deprecated `CoroSession` and `AsyncResult`.
- Deprecated `error::Decode`
- Deprecated the `Session::cancel` overloads taking `CallCancellation`.
- Deprecated `Outcome::deferred`.


### Breaking Changes

- Bumped Boost version requirements to 1.77 to support Asio cancellation slots.
- Errors due to attempting to perform an asynchronous `Session` operation during
  an invalid state are now emitted via the `ErrorOr` passed to the handler,
  instead of throwing `error::Logic`. This is to avoid `error::Logic` exceptions
  being thrown due to race conditions outside the library user's control (for
  example, calling a remote procedure just as the peer terminates the session).
  This also avoids the complications involved in transporting exceptions to
  coroutines, as well as having two mechanisms for reporting errors from the
  same function.
- `Session::authenticate` no longer throws if the session is not in the
  `SessionState::authenticating` state. Instead, the authentication is discarded
  and a warning is emitted.
- `Session::publish(Pub)` no longer throws if the session is not in the
  `SessionState::established` state. Instead, the publicatioon is discarded
  and a warning is emitted.
- `Session::cancel` no longer throws if the session is not in the
  `SessionState::established` state. Instead, the cancellation is discarded
  and a warning is emitted.
- Numeric values of enumerators following `SesesionErrc::invalidState` have
  been bumped by one.
- `Session::call` no longer returns the request ID. To obtain the request ID,
  use the new `Session::call` overload which takes a `CallChit` out
  parameter by reference.
- The signature of `lookupWampErrorUri` has been changed so that it returns
  whether the corresponding error code was found.
- Codec decoders now return a std::error_code instead of throwing an exception.
- The `Transport` type requirement has been changed so that it provides a
  `boost::asio::strand` instead of a `boost::asio::any_executor`.

### Migration Guide

- Replace `AnyExecutor` with `AnyIoExecutor`.
- Replace `AsyncResult` with `ErrorOr`.
- Replace `AsyncResult::get` with `ErrorOr::value`.
- Replace `AsyncResult::errorCode` with `ErrorOr::error`.
- Replace `AsyncResult::setValue` with `ErrorOr::emplace`.
- Replace `Basic[Coro]<Event|Invocation>Unpacker` with
  `Simple[Coro]<Event|Invocation>Unpacker`
- Replace `basic[Coro]<Event|Rpc>` with `simple[Coro]<Event|Rpc>`
- Replace `Cancellation` with `CallCancellation`
- Replace `CancelMode` with `CallCancelMode`
- Replace `CoroSession<>` with `Session`.
- Replace `Outcome::deferred` with `deferment`.
- Replace `#include <cppwamp/corosession.hpp>` with
  `#include <boost/asio/spawn.hpp>` and `#include <cppwamp/session.hpp>`.
- Add `.value()` to `Session` methods taking a `yield_context` to preserve the
  old behavior where either the result value is returned upon success or an
  exception is thrown upon failure.
- `std::error_code` pointers cannot be passed to the the consolidated `Session`
  class. Instead check the returned `ErrorOr` result via `operator bool` and
  `AsyncResult::error()`.
- `Session::call` no longer returns the RPC request ID. Instead use the
  `Session::call` overload which takes a `CallChit` out parameter by reference.
  Alternatively, you may bind an Asio cancellation slot to the completion token.
- Replace `Session::cancel(CallCancellation)` usages with
  `Session::cancel(CallChit)`.
- If used directly, check the `std::error_code` returned by codec decoders
  instead of catching `error::Decode` exceptions.
- Replace the `CppWAMP::coro-headers` CMake target with `CppWAMP::coro-usage`.


v0.9.2
======
Fixed the non-compilation of examples.


v0.9.1
======
Add -fPIC when building vendorized static Boost libraries.


v0.9.0
======
Migrated to jsoncons for all serialization.

- Support for CBOR has been added.
- Codecs have been split into encoders and decoders that can be instantiated
  and reused for multiple encoding/decoding operations.
- Simplified passing of encoded WAMP messages between Peer and AsioTransport.
- Added `toString` free functions for dumping `Variant`, `Array`, and `Object`
  as a JSON-formatted `std::string`.

### Breaking Changes

- The `Codec` type requirements have changed. It affects those who used codecs
  to encode/decode variants outside of the `Session` APIs. This also affects
  those who have extended CppWAMP to use their own custom codecs.
- The `Transport` type requirements have changed. It affects those who
  extended CppWAMP to use their own custom transports.
- Variant instances are output as true JSON via
  `operator(ostream&, const Variant&)`. That means strings variants are
  now output with quotes. Blob variants are now also output with quotes, along
  with a \u0000 prefix, as if they were being transmitted over WAMP.
- Session warnings no longer output to `std::cerr` by default.
  `Session::setWarningHandler` must be explicitly called to re-enable this
  behavior.


v0.8.0
======
Refactored WAMP message processing.

- Payloads/options for Session commands are now directly stored in WampMessage
- Added WampMessage subclasses responsible for marshalling message fields
- Consolidate peer/session data objects to the same C++ module
- Print WAMP message names in traces
- Defunct HEARTBEAT message no longer recognized as valid
- More selective inclusion of Msgpack-c headers
- Add config for older Crossbar versions (for unit tests)
- Perform move capture wherever possible
- Perform automatic enum<->variant conversions while allowing
  custom conversion of specific enum types
- Callbacks can now be registered for session state change events
- Added caller-initiated timeout support
- Support progressive call results for caller
- Added authentication tutorial
- Enriched authentication-related API to handle CRA and SCRAM
  (user must still compute cryto signatures using a 3rd party library)
- Added example programs using the async API
- Added more practical examples of scoped registrations/subscriptions in
  documentation
- Added session logging tutorial
- Added example of converting nested objects in documentation
- Added session leave overloads that don't require a Reason


v0.7.0
======
Migrated to newer Boost.Asio, CMake, and Catch.

### Dependencies

- Migrated to newer Boost.Asio: Boost 1.74 or above now required.
- Removed Boost.Endian dependency.
- Migrated test framework to Catch2: version 2.3 or greater now required.
- RapidJSON and Msgpack-c dependencies are no longer mandatory if their respective codecs are not needed.
- CMake minimum version is now 3.12.
- Migrated to newer Doxygen for generating docs (tested with version 1.8.17)

### CMake

- Overhauled CMake build to adopt modern practices.
- CMake package config now provided when built and installed.
- Separate CMake targets now provided for easy import into another CMake project: `CppWAMP::headers`, `CppWAMP::core`, `CppWAMP::json`, `CppWAMP::msgpack`, and `CppWAMP::coro`.

### Breaking API Changes

- `AsioService` now aliases `boost::asio::io_context`
- `iosvc()` method in `Event`, `Invocation`, and `Interruption` replaced with `executor()` method.
- Signed/unsigned comparisons of numeric `Variants` are now performed correctly (in the mathematical sense).
- Removed the following deprecated methods: `Pub::withBlacklist`, `Pub::withWhiteList`, `Rpc::withBlacklist`, `Rpc::withWhitelist`, `Rpc::withExcludeMe`

### Other API Changes

- Added API visibility macros for shared library builds.
- `Session`, `CoroSession`, and `connect(...)` now have overloads that accept boost::asio::any_executor.
- Added `TcpOptions` and `UdsOptions` which encapsulate socket options.
- `TcpHost` and `UsdPath` now prefer to take socket options via `TcpOptions` and `UdsOptions` respectively in their constructors.
- Allow passing `SO_OOBINLINE` socket option.
- `wamp::ValueTypeOf` now mimics `std::remove_cvref_t` instead of `std::decay_t`.

### Miscellaneous

- Removed git submodules in favor of CMake FetchContent.
- Removed vestigial Qt Creator and Mercurial stuff.
- Fixed json.hpp and msgpack.hpp leaking internals in `CPPWAMP_COMPILED_LIB` mode.
- Removed CPPWAMP_TESTING_FOO macros in favor of Catch2 runtime tags.
- WAMP tests now use any available codec.
- Made header files self-contained to avoid clangd error messages.
- Fixed `-Wall` warnings.
- An `AUTHENTICATE` message with empty signature is now sent to a router if a `CHALLENGE` message is received and there is no registered challenge handler. This is to prevent deadlocking.
- Installation directions are now in README instead of GitHub wiki.
- Tutorials are now located in repo instead of GitHub wiki.

v0.6.3
======
Update for latest 3rd-party dependencies.

- Updated 3rd-party subrepos.
- Fixed missing RawNumber for RapidJSON parser (fixes #100).
- Added missing spaces in conversion exception messages (fixes #101).

v0.6.2
======
Variant conversion enhancements.

- ConversionAccess can now access private default constructors (closes #98).
- To/FromVariantConverter now throws error::Conversion exclusively.
- All RPC argument conversion failures are now propagated back to caller
  (fixes #97).
- Added test cases for bad From/ToVariantConverter conversions.
- Enforced Client::LocalSubs non-empty invariant during unsubscribe.
- Added Variant::at accessors (closes #95).
- Updated config.json test/example files for Crossbar 0.13.0.
- Blob is now stored via a pointer within the Variant::Field union, to reduce
  the size of a Variant object.

v0.6.1
======
Bug fixes.

- Fixed encoding of multibyte UTF-8 sequences to JSON (fixes #96).
- Added test case for converting multibyte UTF-8 sequences to JSON.

v0.6.0
======
Better support for asynchronous RPC and event handlers.

Breaking Changes:
- `Session` and `CoroSession` now take an extra `boost::asio::io_service`
  argument in their `create()` functions. This IO service is now used for
  executing user-provided handlers. It can be the same one used by the
  transport connectors.
- Support for non-handshaking raw socket transports has been
  removed (closes #92).

Enhancements:
- Added `basicCoroRpc()`, `basicCoroEvent()`, `unpackedCoroRpc()`, and
  `unpackedCoroEvent()` wrappers, which execute a call/event slot within the
  context of a coroutine. This should make it easier to implement RPC/event
  handlers that need to run asynchronously themselves (closes #91).
- `Invocation` and `Event` now have an `iosvc()` getter, which returns the
  user-provided `asio::io_service` (closes #91).
- Added `Variant` conversion facilities for `std::set` and `set::unordered_set`.

v0.5.3
======
Fixes and enhancements.

- Added support for binary WAMP payloads (closes #50)
- Fixed wrong header guard in <cppwamp/types/unorderedmap.hpp> (fixes #73)
- Added `SessionErrc` mappings to new predefined error URIs added to
  the spec (closes #77)
- Added recently proposed `exclude_authid`, `exclude_authrole`,
  `eligible_authid`, `eligible_authrole` options (closes #80).
- Fixed `ScopedRegistration`s and `ScopedSubscription`s that weren't being
  moved properly (fixes #82).
- Added `basicRpc` and `basicEvent` functions. `basicRpc` allows the
  registration of statically-typed RPC handlers that don't take an `Invocation`
  parameter and don't return an `Outcome` result. Likewise, `basicEvent` allows
  the registration of statically-typed event handlers that don't take an
  `Event` parameter (closes #84).

v0.5.2
======
More fixes for v0.5.0 release.

- Fixed JSON encoding of control characters (fixes #72)
- Added test case for converting derived DTO classes (see #70)

v0.5.1
======
Minor fixes and enhancements to v0.5.0 release.

- Fixed namespace-related errors that occur for user-defined conversions.
- Can now specify a fallback value when extracting an object member during conversion.
- Added Rpc::captureError which allows users to retrieve ERROR message details returned by a callee.
- Variant-to-Variant conversion is now handled properly.

v0.5.0
======
User-defined type support.

### New Features
- Users may now register custom types that can be converted to/from `Variant`. These registered custom types may be passed directly to RPC and pub/sub operations and handlers. See *Custom Variant Conversions* in the tutorial for usage examples (closes #69).
- `timeservice` and `timeclient` examples have been provided which showcase the use of conversion facilities. These examples use the CppWAMP library in a header-only fashion (closes #67).
- Converters have been provided for `std::unordered_map` and `boost::optional` (closes #68).

### Breaking Changes
- `Payload::withArgs` now takes variadic arguments, instead of a `std::initializer_list<Variant>`. This change makes it possible for registered user-defined types to be automatically converted to `Variant`. Wherever you do `Rpc("foo").withArgs({"hello", 42})` should be changed to `Rpc("foo").withArgs("hello", 42)` (notice the removed curly braces).
- `std::tuple` support is now provided via the new conversion facilities, in `<cppwamp/types/tuple.hpp>`.

### Other Changes
- Fixed compile errors that occur only when the library is used in a header-only fashion.

v0.4.0
======
Connection API improvements.

### Breaking Changes
- `TcpConnector` and `UdsConnector` have been replaced with `connector` factory functions in `<cppwamp/tcp.hpp>` and `<cppwamp/uds.hpp>`. The new interface uses a fluent API which allows the user to specify `setsockopt` socket options (closes #63). Consult the revised tutorials and documentation to learn how to migrate your app code to the revised connection interface.
- Revised connection API so that unused serialization libraries are not needed when using CppWAMP in a header-only fashion (fixes #64).

### Other Changes
- Raw socket transports now use 16MB as the default maximum length for incoming messages (closes #39).

v0.3.1
======
Fixes and additional tests.

### Details
- Added as many test cases as possible for supported advanced WAMP features. Some features cannot be tested because they are not supported on Crossbar, or they are not "symmetrically" supported on CppWAMP (closes #43).
- Added test cases where asynchronous Session operations are executed within call/event slots (closes #44).
- Made changes to allow building with Clang on OS X (thanks taion!) (fixes #55).
- Added test case for constructing `Variant` from `std::vector` (closes #58).
- `Rpc`, `Procedure`, `Pub`, `Topic`, and friends now have converting (implicit) constructors (closes #60).
- NaN and infinite `Real` values are now encoded as `null` over JSON (fixes #61).
- Fixed `unpackedEvent` and `unpackedRpc` so that they can take a lambda functions by value.
- Reorganized `wamptest.cpp` so that related test cases are grouped in `SCENARIO` blocks. The Crossbar router process can no longer be forked from the test suite because of this.

v0.3.0
======
Made further refinements to the API. The minimal Boost library version required is now 1.58.0.

### Details
- The library now depends on Boost 1.58, which now includes Boost.Endian. This removes the dependency on the "standalone" Boost.Endian (closes #5).
- Revamped subscriptions and registrations to more closely model Boost.Signals2 connection management. Users are no longer forced to keep `Subscription`/`Registration` objects alive. `ScopedSubscription` and `ScopedRegistraton` have been added to permit automatic lifetime management, if desired (closes #45).
- RPC handlers are now required to return an `Outcome` object. This makes it harder to forget returning a result from within an RPC handler (closes #46).
- Statically-typed call/event handlers are now handled by `EventUnpacker` and `InvocationUnpacker` wrappers. This eliminates the need for `Session::subscribe` and `Session::enroll` overloads, and greatly simplifies subscription/registration management (closes #51).
- Unpacking of positional arguments for statically-typed call/event slots now uses a simpler technique inspired by `std::integer_sequence` (closes #52)
- Updated examples and tests to use raw socket handshaking, which is now supported on Crossbar (closes #54).

v0.2.0
======
Overhauled API to make use of fluent API techniques. Some Advanced WAMP Profile features are now supported.

### Details
- Renamed some classes:
  - `Client` -> `Session`
  - `CoroClient` -> `CoroSession`
  - `internal::Session` -> `internal::Dialogue`
  - `internal::ClientImplBase` -> `internal::ClientInterface`
  - `internal::ClientImpl` -> `internal::Client`
- Removed the `Args` class, as similar functionality is now provided by `Payload` with a cleaner API
- Folded `CoroErrcClient` into `CoroClient` (now `SessionClient`) by taking an extra, optional `std::error_code` pointer
- `Session` (formerly `Client`) now makes extensive use of fluent API techniques, via data objects declared in `<cppwamp/dialoguedata.hpp>` and `<cppwamp/sessiondata.hpp>` (fixes #6, fixes #7, fixes #34)
- `Subscription` and `Registration` are now returned via shared pointer, and are no longer "handle" objects that mimic a shared pointer (fixes #40).
- Added `operator[]` support to `Variant`, to make it more convenient to access elements/members.
- Added `Variant::valueOr` to make it easier to treat a Variant as an optional value.
- Added support for the "low-hanging fruit" among advanced WAMP features. These are features that only require some `Details` or `Options` attributes to be set, and require no other special treatment by the client (fix #10, fix #11, fix #12, fix #13, fix #14, fix #15, fix #23, fix #24, fix #25, fix #26, fix #27, fix #35, fix #37, fix #38):
  - General: agent identification, feature announcement
  - _Callee_: `call_trustlevels`, `caller_identification`, `pattern_based_registration`, progressive_call_results
  - _Caller_: `call_timeout`, `callee_blackwhite_listing`, `caller_exclusion`, `caller_identification`
  - _Publisher_: `publisher_exclusion`, `publisher_identification`, `subscriber_blackwhite_listing`
  - _Subscriber_: `pattern_based_subscription`, `publication_trustlevels`, `publisher_identification`

v0.1.2
======
Initial public release.

* * *
_Copyright © Butterfly Energy Systems, 2014-2015_
