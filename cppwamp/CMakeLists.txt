#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2014-2015, 2017, 2022-2023.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

set(HEADERS
    include/cppwamp/accesslogging.hpp
    include/cppwamp/any.hpp
    include/cppwamp/anyhandler.hpp
    include/cppwamp/api.hpp
    include/cppwamp/asiodefs.hpp
    include/cppwamp/authinfo.hpp
    include/cppwamp/authorizer.hpp
    include/cppwamp/authenticator.hpp
    include/cppwamp/blob.hpp
    include/cppwamp/cachingauthorizer.hpp
    include/cppwamp/calleestreaming.hpp
    include/cppwamp/callerstreaming.hpp
    include/cppwamp/cancellation.hpp
    include/cppwamp/clientinfo.hpp
    include/cppwamp/codec.hpp
    include/cppwamp/config.hpp
    include/cppwamp/connector.hpp
    include/cppwamp/connectioninfo.hpp
    include/cppwamp/conversionaccess.hpp
    include/cppwamp/corounpacker.hpp
    include/cppwamp/directsession.hpp
    include/cppwamp/disclosure.hpp
    include/cppwamp/errorcodes.hpp
    include/cppwamp/erroror.hpp
    include/cppwamp/errorinfo.hpp
    include/cppwamp/exceptions.hpp
    include/cppwamp/features.hpp
    include/cppwamp/flags.hpp
    include/cppwamp/listener.hpp
    include/cppwamp/logging.hpp
    include/cppwamp/messagebuffer.hpp
    include/cppwamp/null.hpp
    include/cppwamp/options.hpp
    include/cppwamp/payload.hpp
    include/cppwamp/pubsubinfo.hpp
    include/cppwamp/rawsockoptions.hpp
    include/cppwamp/realm.hpp
    include/cppwamp/realmobserver.hpp
    include/cppwamp/registration.hpp
    include/cppwamp/router.hpp
    include/cppwamp/routeroptions.hpp
    include/cppwamp/rpcinfo.hpp
    include/cppwamp/session.hpp
    include/cppwamp/sessioninfo.hpp
    include/cppwamp/spawn.hpp
    include/cppwamp/streaming.hpp
    include/cppwamp/subscription.hpp
    include/cppwamp/tagtypes.hpp
    include/cppwamp/timeout.hpp
    include/cppwamp/traits.hpp
    include/cppwamp/transport.hpp
    include/cppwamp/unpacker.hpp
    include/cppwamp/uri.hpp
    include/cppwamp/variant.hpp
    include/cppwamp/variantdefs.hpp
    include/cppwamp/version.hpp
    include/cppwamp/visitor.hpp
    include/cppwamp/wampdefs.hpp
    include/cppwamp/authenticators/anonymousauthenticator.hpp
    include/cppwamp/bundled/tessil_htrie/htrie_map.h
    include/cppwamp/bundled/sevmeyer_prng.hpp
    include/cppwamp/codecs/cbor.hpp
    include/cppwamp/codecs/json.hpp
    include/cppwamp/codecs/msgpack.hpp
    include/cppwamp/internal/authorizationlistener.hpp
    include/cppwamp/internal/base64.hpp
    include/cppwamp/internal/broker.hpp
    include/cppwamp/internal/challenger.hpp
    include/cppwamp/internal/client.hpp
    include/cppwamp/internal/clientcontext.hpp
    include/cppwamp/internal/commandinfo.hpp
    include/cppwamp/internal/connectioninfoimpl.hpp
    include/cppwamp/internal/dealer.hpp
    include/cppwamp/internal/disclosuremode.hpp
    include/cppwamp/internal/directpeer.hpp
    include/cppwamp/internal/endian.hpp
    include/cppwamp/internal/jsonencoding.hpp
    include/cppwamp/internal/hashcombine.hpp
    include/cppwamp/internal/invocationpolicyoption.hpp
    include/cppwamp/internal/lrucache.hpp
    include/cppwamp/internal/matchpolicyoption.hpp
    include/cppwamp/internal/message.hpp
    include/cppwamp/internal/messagetraits.hpp
    include/cppwamp/internal/metaapi.hpp
    include/cppwamp/internal/networkpeer.hpp
    include/cppwamp/internal/passkey.hpp
    include/cppwamp/internal/peer.hpp
    include/cppwamp/internal/peerlistener.hpp
    include/cppwamp/internal/procedureregistry.hpp
    include/cppwamp/internal/random.hpp
    include/cppwamp/internal/rawsockconnector.hpp
    include/cppwamp/internal/rawsockhandshake.hpp
    include/cppwamp/internal/rawsockheader.hpp
    include/cppwamp/internal/rawsocklistener.hpp
    include/cppwamp/internal/rawsocktransport.hpp
    include/cppwamp/internal/readership.hpp
    include/cppwamp/internal/requestor.hpp
    include/cppwamp/internal/routercontext.hpp
    include/cppwamp/internal/routerimpl.hpp
    include/cppwamp/internal/routerrealm.hpp
    include/cppwamp/internal/routerserver.hpp
    include/cppwamp/internal/routersession.hpp
    include/cppwamp/internal/sessioninfoimpl.hpp
    include/cppwamp/internal/socketoptions.hpp
    include/cppwamp/internal/slotlink.hpp
    include/cppwamp/internal/streamchannel.hpp
    include/cppwamp/internal/surrogateany.hpp
    include/cppwamp/internal/tcpacceptor.hpp
    include/cppwamp/internal/tcpopener.hpp
    include/cppwamp/internal/tcptraits.hpp
    include/cppwamp/internal/timeformatting.hpp
    include/cppwamp/internal/timeoutscheduler.hpp
    include/cppwamp/internal/tokentriemapimpl.hpp
    include/cppwamp/internal/udsacceptor.hpp
    include/cppwamp/internal/udsopener.hpp
    include/cppwamp/internal/udstraits.hpp
    include/cppwamp/internal/variantdecoding.hpp
    include/cppwamp/internal/variantencoding.hpp
    include/cppwamp/internal/varianttraits.hpp
    include/cppwamp/internal/variantvisitors.hpp
    include/cppwamp/transports/tcp.hpp
    include/cppwamp/transports/tcpendpoint.hpp
    include/cppwamp/transports/tcphost.hpp
    include/cppwamp/transports/tcpprotocol.hpp
    include/cppwamp/transports/uds.hpp
    include/cppwamp/transports/udspath.hpp
    include/cppwamp/transports/udsprotocol.hpp
    include/cppwamp/types/array.hpp
    include/cppwamp/types/boostoptional.hpp
    include/cppwamp/types/optional.hpp
    include/cppwamp/types/set.hpp
    include/cppwamp/types/tuple.hpp
    include/cppwamp/types/unorderedmap.hpp
    include/cppwamp/types/unorderedset.hpp
    include/cppwamp/utils/consolelogger.hpp
    include/cppwamp/utils/filelogger.hpp
    include/cppwamp/utils/logtee.hpp
    include/cppwamp/utils/streamlogger.hpp
    include/cppwamp/utils/tokentriemap.hpp
    include/cppwamp/utils/tokentriemapiterator.hpp
    include/cppwamp/utils/tokentriemapnode.hpp
    include/cppwamp/utils/treeview.hpp
    include/cppwamp/utils/triemap.hpp
    include/cppwamp/utils/wildcarduri.hpp
)

set(INLINES
    include/cppwamp/internal/accesslogging.inl.hpp
    include/cppwamp/internal/anonymousauthenticator.inl.hpp
    include/cppwamp/internal/authinfo.inl.hpp
    include/cppwamp/internal/authenticator.inl.hpp
    include/cppwamp/internal/authorizer.inl.hpp
    include/cppwamp/internal/blob.inl.hpp
    include/cppwamp/internal/cachingauthorizer.inl.hpp
    include/cppwamp/internal/calleestreaming.inl.hpp
    include/cppwamp/internal/callerstreaming.inl.hpp
    include/cppwamp/internal/cancellation.inl.hpp
    include/cppwamp/internal/cbor.inl.hpp
    include/cppwamp/internal/connectioninfo.inl.hpp
    include/cppwamp/internal/consolelogger.inl.hpp
    include/cppwamp/internal/clientinfo.inl.hpp
    include/cppwamp/internal/directsession.inl.hpp
    include/cppwamp/internal/errorcodes.inl.hpp
    include/cppwamp/internal/errorinfo.inl.hpp
    include/cppwamp/internal/exceptions.inl.hpp
    include/cppwamp/internal/features.inl.hpp
    include/cppwamp/internal/filelogger.inl.hpp
    include/cppwamp/internal/json.inl.hpp
    include/cppwamp/internal/logging.inl.hpp
    include/cppwamp/internal/messagetraits.inl.hpp
    include/cppwamp/internal/msgpack.inl.hpp
    include/cppwamp/internal/pubsubinfo.inl.hpp
    include/cppwamp/internal/realm.inl.hpp
    include/cppwamp/internal/realmobserver.inl.hpp
    include/cppwamp/internal/registration.inl.hpp
    include/cppwamp/internal/router.inl.hpp
    include/cppwamp/internal/routeroptions.inl.hpp
    include/cppwamp/internal/rpcinfo.inl.hpp
    include/cppwamp/internal/session.inl.hpp
    include/cppwamp/internal/sessioninfo.inl.hpp
    include/cppwamp/internal/streaming.inl.hpp
    include/cppwamp/internal/streamlogger.inl.hpp
    include/cppwamp/internal/subscription.inl.hpp
    include/cppwamp/internal/tcp.inl.hpp
    include/cppwamp/internal/tcpendpoint.inl.hpp
    include/cppwamp/internal/tcphost.inl.hpp
    include/cppwamp/internal/tcpprotocol.inl.hpp
    include/cppwamp/internal/uds.inl.hpp
    include/cppwamp/internal/udspath.inl.hpp
    include/cppwamp/internal/udsprotocol.inl.hpp
    include/cppwamp/internal/variant.inl.hpp
    include/cppwamp/internal/version.inl.hpp
    include/cppwamp/internal/wildcarduri.inl.hpp
)

set(SOURCES
    src/cppwamp.cpp
)

# Minimalist target for header-only use
add_library(cppwamp-core-headers INTERFACE)
target_compile_features(cppwamp-core-headers INTERFACE cxx_std_11)
target_include_directories(cppwamp-core-headers SYSTEM
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(cppwamp-core-headers
    INTERFACE "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
              "$<TARGET_NAME_IF_EXISTS:Boost::system>"
              "$<TARGET_NAME_IF_EXISTS:Threads::Threads>")
if(CPPWAMP_OPT_HEADERS_ONLY)
    target_link_libraries(cppwamp-core-headers
        INTERFACE "$<TARGET_NAME_IF_EXISTS:jsoncons>")
endif()
set_target_properties(cppwamp-core-headers PROPERTIES EXPORT_NAME headers)
add_library(CppWAMP::core-headers ALIAS cppwamp-core-headers)


# Core compiled library target
if(CPPWAMP_OPT_HEADERS_ONLY)
    # Add dummy custom target so that sources are at least listed in IDEs.
    add_custom_target(cppwamp-core-sources
                      SOURCES ${HEADERS} ${INLINES})
else()
    if(NOT APPLE)
        set(CMAKE_INSTALL_RPATH $ORIGIN)
    endif()

    add_library(cppwamp-core ${SOURCES})
    target_sources(cppwamp-core PRIVATE ${HEADERS} ${INLINES})
    target_compile_definitions(cppwamp-core
        PUBLIC
            CPPWAMP_COMPILED_LIB=1
            "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:CPPWAMP_IS_STATIC>")
    target_compile_features(cppwamp-core PUBLIC cxx_std_11)
    target_compile_options(cppwamp-core PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

    # Make it so that warnings are only disabled when CppWAMP is included
    # by an external project.
    target_include_directories(cppwamp-core SYSTEM
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
    target_include_directories(cppwamp-core
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
    target_link_libraries(cppwamp-core
        PUBLIC
            "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
            "$<TARGET_NAME_IF_EXISTS:Boost::system>"
            "$<TARGET_NAME_IF_EXISTS:Threads::Threads>"
        PRIVATE
            "$<TARGET_NAME_IF_EXISTS:jsoncons>")
    set_target_properties(cppwamp-core PROPERTIES
        EXPORT_NAME core
        VERSION ${CppWAMP_VERSION}
        SOVERSION ${CppWAMP_VERSION_MAJOR})

    add_library(CppWAMP::core ALIAS cppwamp-core)
endif()


# Non-source configuration files
add_custom_target(cppwamp-config
    COMMAND ""
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy)
