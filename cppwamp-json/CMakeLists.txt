#-------------------------------------------------------------------------------
#                  Copyright Butterfly Energy Systems 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

set(SOURCES
    src/cppwamp-json.cpp
)

# Minimal interface library for JSON codec usage requirements
add_library(cppwamp-json-headers INTERFACE)
target_compile_features(cppwamp-json-headers INTERFACE cxx_std_11)
target_link_libraries(cppwamp-json-headers
    INTERFACE
        CppWAMP::core-headers
        "$<BUILD_INTERFACE:$<TARGET_NAME_IF_EXISTS:rapidjson>>")
set_target_properties(cppwamp-json-headers PROPERTIES
                      EXPORT_NAME json-headers)
add_library(CppWAMP::json-headers ALIAS cppwamp-json-headers)

if(NOT CPPWAMP_OPT_HEADERS_ONLY)
    if(NOT APPLE)
        set(CMAKE_INSTALL_RPATH $ORIGIN)
    endif()

    add_library(cppwamp-json ${SOURCES})
    target_compile_features(cppwamp-json PUBLIC cxx_std_11)
    target_compile_options(cppwamp-json PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

    # Make it so that warnings are only disabled when CppWAMP is included
    # by an external project.
    target_include_directories(cppwamp-json SYSTEM
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
    target_include_directories(cppwamp-json
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

    # Using BUILD_INTERFACE workaround from
    # https://gitlab.kitware.com/cmake/cmake/-/issues/15415
    target_link_libraries(cppwamp-json
        PUBLIC
            CppWAMP::core
        PRIVATE
            "$<BUILD_INTERFACE:$<TARGET_NAME_IF_EXISTS:rapidjson>>")
    set_target_properties(cppwamp-json PROPERTIES
        EXPORT_NAME json
        VERSION ${CppWAMP_VERSION}
        SOVERSION ${CppWAMP_VERSION_MAJOR})
    add_library(CppWAMP::json ALIAS cppwamp-json)
endif()
