#-------------------------------------------------------------------------------
#                   Copyright Butterfly Energy Systems 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

set(SOURCES
    src/cppwamp-msgpack.cpp
)

# Treat *.ipp files as header files
set_source_files_properties(${INLINES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Minimal interface library for JSON codec usage requirements
add_library(cppwamp-msgpack-headers INTERFACE)
target_compile_features(cppwamp-msgpack-headers INTERFACE cxx_std_11)
target_link_libraries(cppwamp-msgpack-headers
    INTERFACE
        CppWAMP::core-headers
        "$<BUILD_INTERFACE:$<TARGET_NAME_IF_EXISTS:msgpackc-cxx>>")
set_target_properties(cppwamp-msgpack-headers PROPERTIES
                      EXPORT_NAME msgpack-headers)
add_library(CppWAMP::msgpack-headers ALIAS cppwamp-msgpack-headers)

if(NOT CPPWAMP_OPT_HEADERS_ONLY)
    if(NOT APPLE)
        set(CMAKE_INSTALL_RPATH $ORIGIN)
    endif()

    add_library(cppwamp-msgpack ${SOURCES})
    target_sources(cppwamp-msgpack PRIVATE)
    target_compile_features(cppwamp-msgpack PUBLIC cxx_std_11)
    target_compile_options(cppwamp-msgpack PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

    # Make it so that warnings are only disabled when CppWAMP is included
    # by an external project.
    target_include_directories(cppwamp-msgpack SYSTEM
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
    target_include_directories(cppwamp-msgpack
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

    # Using BUILD_INTERFACE workaround from
    # https://gitlab.kitware.com/cmake/cmake/-/issues/15415
    target_link_libraries(cppwamp-msgpack
        PUBLIC
            CppWAMP::core
        PRIVATE
            "$<BUILD_INTERFACE:$<TARGET_NAME_IF_EXISTS:msgpackc-cxx>>")
    set_target_properties(cppwamp-msgpack PROPERTIES
        EXPORT_NAME msgpack
        VERSION ${CppWAMP_VERSION}
        SOVERSION ${CppWAMP_VERSION_MAJOR})
    add_library(CppWAMP::msgpack ALIAS cppwamp-msgpack)
endif()
