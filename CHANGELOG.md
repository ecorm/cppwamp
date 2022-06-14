v0.7.1
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
