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
    include/cppwamp/authorizer.hpp
    include/cppwamp/authenticator.hpp
    include/cppwamp/blob.hpp
    include/cppwamp/cbor.hpp
    include/cppwamp/chits.hpp
    include/cppwamp/codec.hpp
    include/cppwamp/config.hpp
    include/cppwamp/connector.hpp
    include/cppwamp/conversionaccess.hpp
    include/cppwamp/corounpacker.hpp
    include/cppwamp/error.hpp
    include/cppwamp/erroror.hpp
    include/cppwamp/json.hpp
    include/cppwamp/localsession.hpp
    include/cppwamp/listener.hpp
    include/cppwamp/logging.hpp
    include/cppwamp/messagebuffer.hpp
    include/cppwamp/msgpack.hpp
    include/cppwamp/null.hpp
    include/cppwamp/options.hpp
    include/cppwamp/payload.hpp
    include/cppwamp/peerdata.hpp
    include/cppwamp/rawsockoptions.hpp
    include/cppwamp/registration.hpp
    include/cppwamp/router.hpp
    include/cppwamp/routerconfig.hpp
    include/cppwamp/session.hpp
    include/cppwamp/spawn.hpp
    include/cppwamp/subscription.hpp
    include/cppwamp/tagtypes.hpp
    include/cppwamp/tcp.hpp
    include/cppwamp/tcpendpoint.hpp
    include/cppwamp/tcphost.hpp
    include/cppwamp/tcpprotocol.hpp
    include/cppwamp/traits.hpp
    include/cppwamp/transport.hpp
    include/cppwamp/uds.hpp
    include/cppwamp/udspath.hpp
    include/cppwamp/udsprotocol.hpp
    include/cppwamp/unpacker.hpp
    include/cppwamp/variant.hpp
    include/cppwamp/variantdefs.hpp
    include/cppwamp/version.hpp
    include/cppwamp/visitor.hpp
    include/cppwamp/wampdefs.hpp
    include/cppwamp/authenticators/anonymousauthenticator.hpp
    include/cppwamp/bundled/tessil_htrie/htrie_map.h
    include/cppwamp/bundled/sevmeyer_prng.hpp
    include/cppwamp/internal/anonymousauthenticator.ipp
    include/cppwamp/internal/base64.hpp
    include/cppwamp/internal/broker.hpp
    include/cppwamp/internal/callee.hpp
    include/cppwamp/internal/caller.hpp
    include/cppwamp/internal/callertimeout.hpp
    include/cppwamp/internal/challengee.hpp
    include/cppwamp/internal/challenger.hpp
    include/cppwamp/internal/client.hpp
    include/cppwamp/internal/dealer.hpp
    include/cppwamp/internal/endian.hpp
    include/cppwamp/internal/integersequence.hpp
    include/cppwamp/internal/jsonencoding.hpp
    include/cppwamp/internal/localsessionimpl.hpp
    include/cppwamp/internal/messagetraits.hpp
    include/cppwamp/internal/passkey.hpp
    include/cppwamp/internal/peer.hpp
    include/cppwamp/internal/random.hpp
    include/cppwamp/internal/rawsockconnector.hpp
    include/cppwamp/internal/rawsockhandshake.hpp
    include/cppwamp/internal/rawsockheader.hpp
    include/cppwamp/internal/rawsocklistener.hpp
    include/cppwamp/internal/rawsocktransport.hpp
    include/cppwamp/internal/routercontext.hpp
    include/cppwamp/internal/routerimpl.hpp
    include/cppwamp/internal/routerrealm.hpp
    include/cppwamp/internal/routerserver.hpp
    include/cppwamp/internal/routersession.hpp
    include/cppwamp/internal/socketoptions.hpp
    include/cppwamp/internal/subscriber.hpp
    include/cppwamp/internal/surrogateany.hpp
    include/cppwamp/internal/tcpacceptor.hpp
    include/cppwamp/internal/tcpopener.hpp
    include/cppwamp/internal/timeformatting.hpp
    include/cppwamp/internal/tokentriemapimpl.hpp
    include/cppwamp/internal/udsacceptor.hpp
    include/cppwamp/internal/udsopener.hpp
    include/cppwamp/internal/variantdecoding.hpp
    include/cppwamp/internal/variantencoding.hpp
    include/cppwamp/internal/varianttraits.hpp
    include/cppwamp/internal/variantvisitors.hpp
    include/cppwamp/internal/wampmessage.hpp
    include/cppwamp/types/array.hpp
    include/cppwamp/types/boostoptional.hpp
    include/cppwamp/types/optional.hpp
    include/cppwamp/types/set.hpp
    include/cppwamp/types/tuple.hpp
    include/cppwamp/types/unorderedmap.hpp
    include/cppwamp/types/unorderedset.hpp
    include/cppwamp/utils/consolelogger.hpp
    include/cppwamp/utils/tokentriemap.hpp
    include/cppwamp/utils/tokentriemapiterator.hpp
    include/cppwamp/utils/tokentriemapnode.hpp
    include/cppwamp/utils/treeview.hpp
    include/cppwamp/utils/triemap.hpp
    include/cppwamp/utils/wildcarduri.hpp
)

set(INLINES
    include/cppwamp/internal/accesslogging.ipp
    include/cppwamp/internal/authenticator.ipp
    include/cppwamp/internal/authorizer.ipp
    include/cppwamp/internal/authinfo.ipp
    include/cppwamp/internal/blob.ipp
    include/cppwamp/internal/cbor.ipp
    include/cppwamp/internal/chits.ipp
    include/cppwamp/internal/consolelogger.ipp
    include/cppwamp/internal/error.ipp
    include/cppwamp/internal/json.ipp
    include/cppwamp/internal/localsession.ipp
    include/cppwamp/internal/logging.ipp
    include/cppwamp/internal/messagetraits.ipp
    include/cppwamp/internal/msgpack.ipp
    include/cppwamp/internal/peerdata.ipp
    include/cppwamp/internal/registration.ipp
    include/cppwamp/internal/router.ipp
    include/cppwamp/internal/routerconfig.ipp
    include/cppwamp/internal/session.ipp
    include/cppwamp/internal/subscription.ipp
    include/cppwamp/internal/tcp.ipp
    include/cppwamp/internal/tcpendpoint.ipp
    include/cppwamp/internal/tcphost.ipp
    include/cppwamp/internal/tcpprotocol.ipp
    include/cppwamp/internal/uds.ipp
    include/cppwamp/internal/udspath.ipp
    include/cppwamp/internal/udsprotocol.ipp
    include/cppwamp/internal/variant.ipp
    include/cppwamp/internal/version.ipp
    include/cppwamp/internal/wildcarduri.ipp
)

set(SOURCES
    src/cppwamp.cpp
)

# Treat *.ipp files as header files
set_source_files_properties(${INLINES} PROPERTIES HEADER_FILE_ONLY TRUE)

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
