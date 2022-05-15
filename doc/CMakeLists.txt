#-------------------------------------------------------------------------------
#             Copyright Butterfly Energy Systems 2014-2015, 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

set(CPPWAMP_DOCUMENTS
    Doxyfile.in
    concepts.dox
    cppwamp.dox
    registrations.dox
    subscriptions.dox
    tutorial-variants.dox
    tutorial-connectors.dox
    tutorial-sessions.dox
    tutorial-errors.dox
    tutorial-rpc.dox
    tutorial-pubsub.dox
    tutorial-async.dox
    tutorial-conversions.dox
)

find_package(Doxygen)
if(DOXYGEN_FOUND AND NOT CPPWAMP_OPT_HEADERS_ONLY)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
        ${CppWAMP_BINARY_DIR}/Doxyfile
        @ONLY)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/index.html
        ${CppWAMP_BINARY_DIR}/doc/index.html
        COPYONLY)
    add_custom_target(cppwamp-doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CppWAMP_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CppWAMP_SOURCE_DIR}
        SOURCES ${CPPWAMP_DOCUMENTS}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
else()
    add_custom_target(cppwamp-doc-sources
        COMMAND ""
        SOURCES ${CPPWAMP_DOCUMENTS})
endif()
