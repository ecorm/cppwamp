#-------------------------------------------------------------------------------
#           Copyright Butterfly Energy Systems 2014-2015, 2018, 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

# Inspired by https://github.com/alexreinking/SharedStaticStarter

cmake_minimum_required(VERSION 3.12)

include(CMakeFindDependencyMacro)

set(valid_components
    core
    core-headers
    coro-headers
    json
    json-headers
    msgpack
    msgpack-headers)
set(CPPWAMP_MINIMUM_BOOST_VERSION 1.74.0)
set(CPPWAMP_MINIMUM_MSGPACK_VERSION 1.0.0)

set(${CMAKE_FIND_PACKAGE_NAME}_FOUND FALSE)

set(input_component_list "${${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS}")
foreach(component ${input_component_list})
    if(NOT component IN_LIST valid_components)
        set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
            "'${component}' is not a supported CppWAMP component.")
        return()
    endif()
endforeach()

set(core_deps            core core-headers)
set(coro-headers_deps    core-headers coro-headers)
set(json_deps            core core-headers json json-headers)
set(json-headers_deps    core-headers json-headers)
set(msgpack_deps         core core-headers msgpack msgpack-headers)
set(msgpack-headers_deps core-headers msgpack-headers)

list(APPEND component_list core-headers)
foreach(component ${input_component_list})
    list(APPEND component_list "${${component}_deps}")
endforeach ()
list(REMOVE_DUPLICATES component_list)

set(required "${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED}")
set(quietly "${${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY}")

if(coro-headers IN_LIST component_list)
    if("${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_coro-headers}")
        find_dependency(Boost ${CPPWAMP_MINIMUM_BOOST_VERSION}
                        COMPONENTS coroutine context thread system)
    else()
        if(required)
            find_dependency(Boost ${CPPWAMP_MINIMUM_BOOST_VERSION}
                            COMPONENTS system
                            OPTIONAL_COMPONENTS coroutine context thread)
        elseif(quietly)
            find_package(Boost ${CPPWAMP_MINIMUM_BOOST_VERSION}
                         QUIET
                         COMPONENTS system
                         OPTIONAL_COMPONENTS coroutine context thread)
        else()
            find_package(Boost ${CPPWAMP_MINIMUM_BOOST_VERSION}
                         COMPONENTS system
                         OPTIONAL_COMPONENTS coroutine context thread)
        endif()
    endif()
else()
    find_dependency(Boost ${CPPWAMP_MINIMUM_BOOST_VERSION}
                    COMPONENTS system)
endif()

if(json-headers IN_LIST component_list)
    if("${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_json}" OR
       "${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_json-headers}")
        find_dependency(RapidJSON)
    elseif(quietly)
        find_package(RapidJSON QUIET)
    else()
        find_package(RapidJSON)
    endif()
endif()

if(msgpack-headers IN_LIST component_list)
    if("${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_msgpack}" OR
       "${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_msgpack-headers}")
        find_dependency(Msgpack)
    elseif(quietly)
        find_package(Msgpack QUIET)
    else()
        find_package(Msgpack)
    endif()
endif()

macro(load_header_target name)
    if("${name}-headers" IN_LIST component_list)
        set(target_file
            "${CMAKE_CURRENT_LIST_DIR}/cppwamp-${name}-headers-target.cmake")
        if(NOT EXISTS ${target_file}
           AND "${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_${name}-headers}")
            set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
                "CppWAMP ${name}-headers target was not found.")
            return()
        endif()
        if(EXISTS ${target_file})
            include(${target_file})
        endif()
    endif()
endmacro()

set(linkage static)
if(DEFINED CPPWAMP_OPT_SHARED_LIBS)
    if(CPPWAMP_OPT_SHARED_LIBS)
        set(linkage shared)
    endif()
elseif(BUILD_SHARED_LIBS)
    set(linkage shared)
endif()

macro(load_target name)
    if("${name}" IN_LIST component_list)
        set(target_file
            "${CMAKE_CURRENT_LIST_DIR}/cppwamp-${linkage}-${name}-target.cmake")
        if(NOT EXISTS ${target_file}
           AND "${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_${name}}")
            set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
                "CppWAMP ${name} target was not found.")
            return()
        endif()
        if(EXISTS ${target_file})
            include(${target_file})
        endif()
    endif()
endmacro()

load_header_target(core)
load_header_target(json)
load_header_target(msgpack)
load_header_target(coro)

load_target(core)
load_target(json)
load_target(msgpack)

set(${CMAKE_FIND_PACKAGE_NAME}_FOUND TRUE)
