/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COMPILED_LIB
#error CPPWAMP_COMPILED_LIB must be defined to use this source file
#endif

#include <cppwamp/config.hpp>

#include <cppwamp/internal/blob.ipp>
#include <cppwamp/internal/cbor.ipp>
#include <cppwamp/internal/chits.ipp>
#include <cppwamp/internal/connector.ipp>
#include <cppwamp/internal/error.ipp>
#include <cppwamp/internal/json.ipp>
#include <cppwamp/internal/messagetraits.ipp>
#include <cppwamp/internal/msgpack.ipp>
#include <cppwamp/internal/peerdata.ipp>
#include <cppwamp/internal/registration.ipp>
#include <cppwamp/internal/session.ipp>
#include <cppwamp/internal/subscription.ipp>
#include <cppwamp/internal/tcp.ipp>
#include <cppwamp/internal/tcphost.ipp>
#include <cppwamp/internal/variant.ipp>
#include <cppwamp/internal/version.ipp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/internal/uds.ipp>
    #include <cppwamp/internal/udspath.ipp>
#endif
