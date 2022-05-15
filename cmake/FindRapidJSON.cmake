#-------------------------------------------------------------------------------
#                  Copyright Butterfly Energy Systems 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

#[=======================================================================[.rst:
FindRapidJSON
-------------

Finds the RapidJSON library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``rapidjson``
  The RapidJSON header-only interface library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``RapidJSON_FOUND``
  True if the system has the RapidJSON headers.
``RapidJSON_VERSION``
  The version of the RapidJSON library which was found.
``RapidJSON_INCLUDE_DIRS``
  Include directories needed to use RapidJSON.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``RapidJSON_INCLUDE_DIR``
  The directory containing ``rapidjson/rapidjson.h``.

Hints
^^^^^

This module reads hints about search locations from variables:

``RapidJSON_ROOT``
  Preferred installation prefix.

#]=======================================================================]

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_RapidJSON QUIET RapidJSON)
endif()

find_path(RapidJSON_INCLUDE_DIR "rapidjson/rapidjson.h"
    HINTS ${PC_RapidJSON_INCLUDE_DIRS} ${PC_RapidJSON_INCLUDEDIR}
)
set(RapidJSON_VERSION ${PC_RapidJSON_VERSION})

set(RapidJSON_VERSION_FILE
    "${RapidJSON_INCLUDE_DIR}/rapidjson/rapidjson.h")
if(NOT "${RapidJSON_INCLUDE_DIR}" STREQUAL "RapidJSON_INCLUDE_DIR-NOTFOUND"
   AND "${RapidJSON_VERSION}" STREQUAL ""
   AND EXISTS ${RapidJSON_VERSION_FILE})
    # Expected format:
    #define RAPIDJSON_MAJOR_VERSION N
    #define RAPIDJSON_MINOR_VERSION N
    #define RAPIDJSON_PATCH_VERSION N
    file(STRINGS ${RapidJSON_VERSION_FILE} RapidJSON_VERSION_LINES
         REGEX "#define RAPIDJSON_[A-Z]+_VERSION"
         LIMIT_COUNT 3)
    list(LENGTH RapidJSON_VERSION_LINES RapidJSON_VERSION_LINE_COUNT)
    if(${RapidJSON_VERSION_LINE_COUNT} GREATER 2)
        list(GET RapidJSON_VERSION_LINES 0 RapidJSON_VMAJ_LN)
        list(GET RapidJSON_VERSION_LINES 1 RapidJSON_VMIN_LN)
        list(GET RapidJSON_VERSION_LINES 2 RapidJSON_VREV_LN)
        string(REGEX MATCH "[0-9]" RapidJSON_VMAJ ${RapidJSON_VMAJ_LN})
        string(REGEX MATCH "[0-9]" RapidJSON_VMIN ${RapidJSON_VMIN_LN})
        string(REGEX MATCH "[0-9]" RapidJSON_VREV ${RapidJSON_VREV_LN})
        set(RapidJSON_VERSION
            "${RapidJSON_VMAJ}.${RapidJSON_VMIN}.${RapidJSON_VREV}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RapidJSON
    FOUND_VAR RapidJSON_FOUND
    REQUIRED_VARS RapidJSON_INCLUDE_DIR
    VERSION_VAR RapidJSON_VERSION
)

if(RapidJSON_FOUND)
    set(RapidJSON_INCLUDE_DIRS ${RapidJSON_INCLUDE_DIR})

    if(NOT TARGET rapidjson)
        if(NOT TARGET rapidjson)
            add_library(rapidjson INTERFACE IMPORTED)
        else()
            add_library(rapidjson ALIAS rapidjson)
        endif()
        target_include_directories(rapidjson SYSTEM
            INTERFACE "${RapidJSON_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(RapidJSON_INCLUDE_DIR)
