#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

set(SOURCES src/cppwamp.cpp)

set(HEADERS
    include/cppwamp/asiodefs.hpp
    include/cppwamp/asyncresult.hpp
    include/cppwamp/blob.hpp
    include/cppwamp/codec.hpp
    include/cppwamp/connector.hpp
    include/cppwamp/conversion.hpp
    include/cppwamp/corosession.hpp
    include/cppwamp/corounpacker.hpp
    include/cppwamp/error.hpp
    include/cppwamp/json.hpp
    include/cppwamp/msgpack.hpp
    include/cppwamp/null.hpp
    include/cppwamp/options.hpp
    include/cppwamp/payload.hpp
    include/cppwamp/peerdata.hpp
    include/cppwamp/rawsockoptions.hpp
    include/cppwamp/registration.hpp
    include/cppwamp/session.hpp
    include/cppwamp/sessiondata.hpp
    include/cppwamp/subscription.hpp
    include/cppwamp/tcp.hpp
    include/cppwamp/tcphost.hpp
    include/cppwamp/traits.hpp
    include/cppwamp/uds.hpp
    include/cppwamp/udspath.hpp
    include/cppwamp/unpacker.hpp
    include/cppwamp/variant.hpp
    include/cppwamp/version.hpp
    include/cppwamp/visitor.hpp
    include/cppwamp/wampdefs.hpp
    include/cppwamp/internal/asynctask.hpp
    include/cppwamp/internal/asioconnector.hpp
    include/cppwamp/internal/asioendpoint.hpp
    include/cppwamp/internal/asiolistener.hpp
    include/cppwamp/internal/asiotransport.hpp
    include/cppwamp/internal/base64.hpp
    include/cppwamp/internal/callee.hpp
    include/cppwamp/internal/client.hpp
    include/cppwamp/internal/clientinterface.hpp
    include/cppwamp/internal/config.hpp
    include/cppwamp/internal/endian.hpp
    include/cppwamp/internal/integersequence.hpp
    include/cppwamp/internal/messagetraits.hpp
    include/cppwamp/internal/passkey.hpp
    include/cppwamp/internal/peer.hpp
    include/cppwamp/internal/precompiled.hpp
    include/cppwamp/internal/rawsockconnector.hpp
    include/cppwamp/internal/rawsockhandshake.hpp
    include/cppwamp/internal/rawsockheader.hpp
    include/cppwamp/internal/subscriber.hpp
    include/cppwamp/internal/tcpacceptor.hpp
    include/cppwamp/internal/tcpopener.hpp
    include/cppwamp/internal/udsacceptor.hpp
    include/cppwamp/internal/udsopener.hpp
    include/cppwamp/internal/variantbuilder.hpp
    include/cppwamp/internal/varianttraits.hpp
    include/cppwamp/internal/varianttraitsfwd.hpp
    include/cppwamp/internal/variantvisitors.hpp
    include/cppwamp/internal/wampmessage.hpp
    include/cppwamp/types/set.hpp
    include/cppwamp/types/tuple.hpp
    include/cppwamp/types/boostoptional.hpp
    include/cppwamp/types/unorderedmap.hpp
    include/cppwamp/types/unorderedset.hpp
)

set(INLINES
    include/cppwamp/internal/asyncresult.ipp
    include/cppwamp/internal/blob.ipp
    include/cppwamp/internal/conversion.ipp
    include/cppwamp/internal/corosession.ipp
    include/cppwamp/internal/corounpacker.ipp
    include/cppwamp/internal/endian.ipp
    include/cppwamp/internal/error.ipp
    include/cppwamp/internal/json.ipp
    include/cppwamp/internal/messagetraits.ipp
    include/cppwamp/internal/msgpack.ipp
    include/cppwamp/internal/null.ipp
    include/cppwamp/internal/options.ipp
    include/cppwamp/internal/payload.ipp
    include/cppwamp/internal/peerdata.ipp
    include/cppwamp/internal/rawsockoptions.ipp
    include/cppwamp/internal/registration.ipp
    include/cppwamp/internal/session.ipp
    include/cppwamp/internal/sessiondata.ipp
    include/cppwamp/internal/subscription.ipp
    include/cppwamp/internal/tcp.ipp
    include/cppwamp/internal/tcphost.ipp
    include/cppwamp/internal/uds.ipp
    include/cppwamp/internal/udspath.ipp
    include/cppwamp/internal/unpacker.ipp
    include/cppwamp/internal/variant.ipp
    include/cppwamp/internal/version.ipp
    include/cppwamp/internal/visitor.ipp
    include/cppwamp/types/internal/tuple.ipp
)

set(DOCUMENTS
    ${PROJECT_SOURCE_DIR}/doc/concepts.dox
    ${PROJECT_SOURCE_DIR}/doc/cppwamp.dox
    ${PROJECT_SOURCE_DIR}/doc/registrations.dox
    ${PROJECT_SOURCE_DIR}/doc/subscriptions.dox
)

# Treat *.ipp files as header files
set_source_files_properties(${INLINES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Add library target
add_library(cppwamp ${SOURCES} ${HEADERS} ${INLINES} ${DOCUMENTS})

# Add preprocessor macro definitions
add_definitions(-DCPPWAMP_COMPILED_LIB)

# Set include path for other targets that are dependant on this library
target_include_directories(cppwamp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(APPLE)
    # Link boost libraries (command is redundant in linux)
    target_link_libraries(cppwamp
        ${Boost_LIBRARIES}
    )
endif()

# Add a GUI variable that let's the user choose if a shared or static library
# will be built.
option(
    BUILD_SHARED_LIBS
    "Determines if a shared or static library will be built"
    ON
)

# Add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(
        ${PROJECT_SOURCE_DIR}/Doxyfile.in
        ${PROJECT_BINARY_DIR}/Doxyfile
        @ONLY
    )
    configure_file(
        ${PROJECT_SOURCE_DIR}/doc/index.html
        ${PROJECT_BINARY_DIR}/doc/index.html
        COPYONLY
    )
    add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif(DOXYGEN_FOUND)
