<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Header-Only Use
===============

This page describes the requirements for using CppWAMP as a header-only
library in your project.

Dependencies
------------

The following third-party libraries are already bundled with CppWAMP as git
submodules, under the `ext` subdirectory:

- [Boost.Endian][boost-endian] (Required)
- [Rapidjson][rapidjson] (Required only if you're using JSON serialization)
- [Msgpack-c][msgpack-c], release [1.0.0][msgpack-c-100] or greater (Required
  only if you're using Msgpack serialization)

The above libraries are used in a header-only fashion and do not need to be
compiled.

You'll also need the following [Boost][boost] libraries (version 1.56 or
greater):
- Boost.Asio (Header-only)
- Boost.System (Compiled)
- Boost.Coroutine (Compiled, Optional &ndash; Needed only for coroutine
  client API)
- Boost.Context (Compiled, Optional &ndash; Needed only for coroutine
  client API)

[boost-endian]: https://github.com/boostorg/endian
[rapidjson]: https://github.com/miloyip/rapidjson
[msgpack-c]: https://github.com/msgpack/msgpack-c
[msgpack-c-100]: https://github.com/msgpack/msgpack-c/releases/tag/cpp-1.0.0
[boost]: http://www.boost.org/

1. Cloning CppWAMP
------------------

The following command will clone CppWAMP and its submodules, placing everything
under a `cppwamp` directory:

```bash
git clone --recursive https://github.com/ecorm/cppwamp
```

2. Building Required Boost Libraries
------------------------------------
**If you already have Boost 1.56.0 or greater installed on your system, you may
skip this step.**

The following steps will place the Boost libraries under the `cppwamp/ext`
subdirectory. Of course, you may install the Boost libraries wherever you want
on your system, as long as you tell your compiler where to find those libraries.

After cloning CppWAMP, navigate to the `cppwamp/ext` subdirectory, download the
latest Boost package, and extract it to `cppwamp/ext/boost`:

```bash
cd cppwamp/ext
wget http://downloads.sourceforge.net/project/boost/boost/1.57.0/boost_1_57_0.tar.bz2
tar xjf boost_1_57_0.tar.bz2
mv boost_1_57_0 boost
```

where `1.57.0` can be replaced by a newer version number, if available. Now go
to the extracted Boost archive and build the required Boost libraries using the
following commands:

```bash
cd boost
./bootstrap.sh --with-libraries=system,coroutine,context
./b2 -j4
```
where ``-j4` specifies a parallel build using 4 jobs. You may want to increase
or decrease the number of parallel jobs, depending on the number of cores
available on your system. If you prefer to build _all_ Boost libraries, then
leave out the `--with-libraries` option.

After building, the Boost library binaries should be available under
`cppwamp/ext/boost/stage/lib`. You can verify this by issuing:

```bash
ls stage/lib
```

<a name="headeronlyopts"></a>
3. Adding Compiler and Linker Options To Your Program
-----------------------------------------------------

After you have obtained CppWAMP and its dependencies, you may use it in your
programs by using the following compiler flags:

- `-std=c++11` (for GCC and Clang compilers)
- `-Ipath/to/cppwamp/cppwamp/include`
- `-Ipath/to/cppwamp/ext/boost`
- `-Ipath/to/cppwamp/ext/endian/include`
- `-Ipath/to/cppwamp/ext/msgpack-c/include`
- `-Ipath/to/cppwamp/ext/rapidjson/include`

as well as the following linker flags:

- `-Lpath/to/cppwamp/ext/boost/stage/lib`
- `-lboost_coroutine`
- `-lboost_context`
- `-lboost_system`

Note that `-lboost_coroutine` and `-lboost_context` are not required if you're
not using the coroutine-based client API.
