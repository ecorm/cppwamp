/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COMPILED_LIB
#error CPPWAMP_COMPILED_LIB must be defined to use this source file
#endif

#include <cppwamp/config.hpp>

#include <cppwamp/internal/accesslogging.ipp>
#include <cppwamp/internal/anonymousauthenticator.ipp>
#include <cppwamp/internal/authorizer.ipp>
#include <cppwamp/internal/authenticator.ipp>
#include <cppwamp/internal/blob.ipp>
#include <cppwamp/internal/calleestreaming.ipp>
#include <cppwamp/internal/callerstreaming.ipp>
#include <cppwamp/internal/cancellation.ipp>
#include <cppwamp/internal/cbor.ipp>
#include <cppwamp/internal/clientinfo.ipp>
#include <cppwamp/internal/consolelogger.ipp>
#include <cppwamp/internal/directsession.ipp>
#include <cppwamp/internal/errorcodes.ipp>
#include <cppwamp/internal/errorinfo.ipp>
#include <cppwamp/internal/exceptions.ipp>
#include <cppwamp/internal/features.ipp>
#include <cppwamp/internal/json.ipp>
#include <cppwamp/internal/logging.ipp>
#include <cppwamp/internal/messagetraits.ipp>
#include <cppwamp/internal/msgpack.ipp>
#include <cppwamp/internal/pubsubinfo.ipp>
#include <cppwamp/internal/realm.ipp>
#include <cppwamp/internal/realmobserver.ipp>
#include <cppwamp/internal/registration.ipp>
#include <cppwamp/internal/router.ipp>
#include <cppwamp/internal/routerconfig.ipp>
#include <cppwamp/internal/rpcinfo.ipp>
#include <cppwamp/internal/session.ipp>
#include <cppwamp/internal/sessioninfo.ipp>
#include <cppwamp/internal/streaming.ipp>
#include <cppwamp/internal/streamlogger.ipp>
#include <cppwamp/internal/subscription.ipp>
#include <cppwamp/internal/tcp.ipp>
#include <cppwamp/internal/tcpendpoint.ipp>
#include <cppwamp/internal/tcphost.ipp>
#include <cppwamp/internal/tcpprotocol.ipp>
#include <cppwamp/internal/variant.ipp>
#include <cppwamp/internal/version.ipp>
#include <cppwamp/internal/wildcarduri.ipp>

#ifdef CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/internal/uds.ipp>
    #include <cppwamp/internal/udspath.ipp>
    #include <cppwamp/internal/udsprotocol.ipp>
#endif
