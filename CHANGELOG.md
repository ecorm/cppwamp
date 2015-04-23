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
