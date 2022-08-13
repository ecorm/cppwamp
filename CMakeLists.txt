#-------------------------------------------------------------------------------
#           Copyright Butterfly Energy Systems 2014-2015, 2018, 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# This project defines one of more of the following targets, depending
# on the how the CPPWAMP_OPT_X options are set:
#
# Target               | Import alias          | All | Description
# -------------------- | --------------------- | --- | -----------
# cppwamp-core         | CppWAMP::core         | Yes | Compiled libcppwamp-core library
# cppwamp-core-headers | CppWAMP::core-headers | Yes | Header-only usage requirements
# cppwamp-coro-usage   | CppWAMP::coro-usage   | Yes | Boost.Coroutine usage requirements
# cppwamp-doc          | <none>                | No  | Doxygen documentation
# cppwamp-examples     | <none>                | No  | Compiled example programs
# cppwamp-test         | <none>                | No  | Compiled test suite program
#
# 'All' means that the target (if enabled) is built as part of the 'all' target.
# 'Usage requirements' means that the appropriate compiler flags will be set
# if you add the target to target_link_libraries in your CMake project.
#
# This project supports inclusion into another CMake project via either
# add_subdirectory(cppwamp) or find_package(CppWAMP ...).
#-------------------------------------------------------------------------------


# Version 3.11 needed for FetchContent
# Version 3.12 needed for find_path's <PackageName>_ROOT support
# Version 3.12 needed for install(TARGETS ... NAMELINK_COMPONENT ...)
# Version 3.12 needed for $<TARGET_EXISTS:target> generator expression
cmake_minimum_required (VERSION 3.12)

# Include guard in case this project becomes a diamond dependency via multiple
# add_directory(cppwamp) calls.
include_guard()

project(CppWAMP
        VERSION 0.10.0
        LANGUAGES CXX)

include(ProcessorCount)
ProcessorCount(CPPWAMP_CORE_COUNT)
if(CPPWAMP_CORE_COUNT EQUAL 0)
    set(CPPWAMP_CORE_COUNT 1)
endif()

get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
       isTopLevel)

#----------------------------- User options ------------------------------------

if(NOT isMultiConfig AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "The type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
        "MinSizeRel" "RelWithDebInfo")
endif()

option(BUILD_SHARED_LIBS
"Determines whether a shared or static library will be built. If already set \
by a superproject, the CPPWAMP_OPT_SHARED_LIBS non-cache variable may be set \
to override the shared/static setting specifically for CppWAMP."
OFF)

option(CPPWAMP_OPT_HEADERS_ONLY
"Don't build libraries/tests/examples and only create targets for \
header-only use."
OFF)

option(CPPWAMP_OPT_VENDORIZE
"Fetches and builds dependencies during configuration. If disabled, the \
dependencies will be searched via find_package. The searching can be \
directed by defining <dependency>_ROOT variables. If the dependent targets are \
already imported by a superproject, find_package calls will be skipped."
OFF)

option(CPPWAMP_OPT_WITH_CORO
"Adds the Boost.Coroutine dependency and build tests and examples \
that depend on it"
ON)

option(CPPWAMP_OPT_WITH_CORO20
"Adds C++20 couroutine examples if examples are enabled"
OFF)

option(CPPWAMP_OPT_WITH_DOCS
"Creates a build target for generating documentation"
${isTopLevel})

option(CPPWAMP_OPT_WITH_TESTS
"Adds the Catch2 dependency and creates a build target for unit tests"
${isTopLevel})

option(CPPWAMP_OPT_WITH_EXAMPLES
"Creates a build target for examples"
${isTopLevel})

option(CPPWAMP_OPT_WITH_PACKAGING
"Generates libary packaging rules"
${isTopLevel})

#--------------------------- Advanced options ----------------------------------

set(CPPWAMP_OPT_BOOST_CONFIG "" CACHE FILEPATH
    "The user-config file to use when building Boost (optional)")
mark_as_advanced(CPPWAMP_OPT_BOOST_CONFIG)

set(CPPWAMP_OPT_BOOST_JOBS ${CPPWAMP_CORE_COUNT} CACHE STRING
    "Number of threads (-j option) to use when building Boost")
mark_as_advanced(CPPWAMP_OPT_BOOST_JOBS)

#------------------------ End of user options ----------------------------------

set(CPPWAMP_PREVIOUSLY_VENDORIZED OFF CACHE INTERNAL "")

if(DEFINED CPPWAMP_OPT_SHARED_LIBS)
    set(BUILD_SHARED_LIBS "${CPPWAMP_OPT_SHARED_LIBS}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Set default export visibility to hidden for all targets
if(NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND
    NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif()

# Fetch or find dependencies
include(CppWAMPDependencies)
cppwamp_resolve_dependencies()

add_subdirectory(cppwamp)

if(CPPWAMP_OPT_WITH_CORO)
    add_subdirectory(cppwamp-coro)
endif()

if(CPPWAMP_OPT_WITH_DOCS AND NOT CPPWAMP_OPT_HEADERS_ONLY)
    add_subdirectory(doc)
endif()

if(CPPWAMP_OPT_WITH_TESTS AND NOT CPPWAMP_OPT_HEADERS_ONLY)
    add_subdirectory(test)
endif()

if(CPPWAMP_OPT_WITH_EXAMPLES AND NOT CPPWAMP_OPT_HEADERS_ONLY)
    add_subdirectory(examples)
endif()

if(CPPWAMP_OPT_WITH_PACKAGING)
    add_subdirectory(packaging)
endif()
