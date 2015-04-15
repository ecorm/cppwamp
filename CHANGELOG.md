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
- `Session` (formerly `Client`) now makes extensive use of fluent API techniques, via data objects declared in `<cppwamp/dialoguedata.hpp>` and `<cppwamp/sessiondata.hpp>` (#6, #7, #34)
- `Subscription` and `Registration` are now returned via shared pointer, and are no longer "handle" objects that mimic a shared pointer (#40).
- Added `operator[]` support to `Variant`, to make it more convenient to access elements/members.
- Added `Variant::valueOr` to make it easier to treat a Variant as an optional value.
- Added support for the "low-hanging fruit" among advanced WAMP features. These are features that only require some `Details` or `Options` attributes to be set, and require no other special treatment by the client (#10, #11, #12, #13, #14, #15, #23, #24, #25, #26, #27, #35, #37, #38):
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
