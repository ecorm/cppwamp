# CppWAMP
C++11 client library for the WAMP protocol.

The first release of this libary should be available sometime in ~~February~~ March 2015.

Features (currently working):
- Supports WAMP Basic Profile
- Roles: Caller, Callee, Subscriber, Publisher
- Transports: TCP and Unix Domain raw sockets (with and without handshaking support)
- Serializations: JSON and MsgPack
- Provides both callback and co-routine based asynchronous APIs
- Easy conversion between static and dynamic types
- Header-only, but may also be optionally compiled
- Permissive license (Boost)
- Unit tested

Dependencies:
- Boost.Asio for raw socket transport
- Boost.Endian
- RapidJSON
- msgpack-c
- (optional) Boost.Coroutine

To do before initial release:
- Write documentation
- Provide examples
- Provide platform-independant build script
