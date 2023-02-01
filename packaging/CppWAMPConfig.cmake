#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2022-2023.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

# Inspired by https://github.com/alexreinking/SharedStaticStarter

cmake_minimum_required(VERSION 3.12)

include(CMakeFindDependencyMacro)

set(valid_components
    core
    core-headers
    coro-usage)
set(CPPWAMP_MINIMUM_BOOST_VERSION 1.81.0)
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

set(core_deps       core core-headers)
set(coro-usage_deps core-headers coro-usage)

list(APPEND component_list core-headers)
foreach(component ${input_component_list})
    list(APPEND component_list "${${component}_deps}")
endforeach ()
list(REMOVE_DUPLICATES component_list)

set(required "${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED}")
set(quietly "${${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY}")

if(coro-usage IN_LIST component_list)
    if("${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_coro-usage}")
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

if(quietly)
    find_package(jsoncons QUIET)
else()
    find_package(jsoncons)
endif()

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
            "${CMAKE_CURRENT_LIST_DIR}/cppwamp-${name}-target.cmake")
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

macro(load_compiled_target name)
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

load_target(core-headers)
load_target(coro-usage)
load_compiled_target(core)

set(${CMAKE_FIND_PACKAGE_NAME}_FOUND TRUE)
