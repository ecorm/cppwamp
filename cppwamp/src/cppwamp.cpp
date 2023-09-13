/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_COMPILED_LIB
#error CPPWAMP_COMPILED_LIB must be defined to use this source file
#endif

#include <cppwamp/config.hpp>

#include <cppwamp/internal/accesslogging.inl.hpp>
#include <cppwamp/internal/anonymousauthenticator.inl.hpp>
#include <cppwamp/internal/authinfo.inl.hpp>
#include <cppwamp/internal/authorizer.inl.hpp>
#include <cppwamp/internal/authenticator.inl.hpp>
#include <cppwamp/internal/blob.inl.hpp>
#include <cppwamp/internal/cachingauthorizer.inl.hpp>
#include <cppwamp/internal/calleestreaming.inl.hpp>
#include <cppwamp/internal/callerstreaming.inl.hpp>
#include <cppwamp/internal/cancellation.inl.hpp>
#include <cppwamp/internal/cbor.inl.hpp>
#include <cppwamp/internal/clientinfo.inl.hpp>
#include <cppwamp/internal/connectioninfo.inl.hpp>
#include <cppwamp/internal/consolelogger.inl.hpp>
#include <cppwamp/internal/directsession.inl.hpp>
#include <cppwamp/internal/errorcodes.inl.hpp>
#include <cppwamp/internal/errorinfo.inl.hpp>
#include <cppwamp/internal/exceptions.inl.hpp>
#include <cppwamp/internal/features.inl.hpp>
#include <cppwamp/internal/filelogger.inl.hpp>
#include <cppwamp/internal/json.inl.hpp>
#include <cppwamp/internal/logging.inl.hpp>
#include <cppwamp/internal/messagetraits.inl.hpp>
#include <cppwamp/internal/msgpack.inl.hpp>
#include <cppwamp/internal/pubsubinfo.inl.hpp>
#include <cppwamp/internal/realm.inl.hpp>
#include <cppwamp/internal/realmobserver.inl.hpp>
#include <cppwamp/internal/registration.inl.hpp>
#include <cppwamp/internal/router.inl.hpp>
#include <cppwamp/internal/routeroptions.inl.hpp>
#include <cppwamp/internal/rpcinfo.inl.hpp>
#include <cppwamp/internal/session.inl.hpp>
#include <cppwamp/internal/sessioninfo.inl.hpp>
#include <cppwamp/internal/streaming.inl.hpp>
#include <cppwamp/internal/streamlogger.inl.hpp>
#include <cppwamp/internal/subscription.inl.hpp>
#include <cppwamp/internal/tcp.inl.hpp>
#include <cppwamp/internal/tcpprotocol.inl.hpp>
#include <cppwamp/internal/variant.inl.hpp>
#include <cppwamp/internal/version.inl.hpp>
#include <cppwamp/internal/wildcarduri.inl.hpp>

#ifdef CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/internal/uds.inl.hpp>
    #include <cppwamp/internal/udsprotocol.inl.hpp>
#endif
