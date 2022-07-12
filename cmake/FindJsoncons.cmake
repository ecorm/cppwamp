#-------------------------------------------------------------------------------
#                 Copyright Butterfly Energy Systems 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

#[=======================================================================[.rst:
FindJsoncons
-----------

Finds the jsoncons C++ library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``jsonconsc-cxx``
  The jsoncons C++ header-only interface library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``jsoncons_FOUND``
  True if the system has the jsoncons C++ headers.
``jsoncons_VERSION``
  The version of the jsoncons C++ library which was found.
``jsoncons_INCLUDE_DIRS``
  Include directories needed to use jsoncons C++.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``jsoncons_INCLUDE_DIR``
  The directory containing ``jsoncons.hpp``.

Hints
^^^^^

This module reads hints about search locations from variables:

``jsoncons_ROOT``
  Preferred installation prefix.

#]=======================================================================]

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_jsoncons QUIET jsoncons)
endif()

find_path(jsoncons_INCLUDE_DIR "jsoncons/jsoncons.hpp"
    HINTS ${PC_jsoncons_INCLUDE_DIRS} ${PC_jsoncons_INCLUDEDIR}
)
set(jsoncons_VERSION ${PC_jsoncons_VERSION})

set(jsoncons_VERSION_FILE
    "${jsoncons_INCLUDE_DIR}/jsoncons/config/version.hpp")
if(NOT "${jsoncons_INCLUDE_DIR}" STREQUAL "jsoncons_INCLUDE_DIR-NOTFOUND"
   AND "${jsoncons_VERSION}" STREQUAL ""
   AND EXISTS ${jsoncons_VERSION_FILE})
    # Expected version_master.hpp file format:
    #define JSONCONS_VERSION_MAJOR N
    #define JSONCONS_VERSION_MINOR N
    #define JSONCONS_VERSION_PATCH N
    file(STRINGS ${jsoncons_VERSION_FILE} jsoncons_VERSION_LINES
         REGEX "#define JSONCONS_VERSION"
         LIMIT_COUNT 3)
    list(LENGTH jsoncons_VERSION_LINES jsoncons_VERSION_LINE_COUNT)
    if(${jsoncons_VERSION_LINE_COUNT} GREATER 2)
        list(GET jsoncons_VERSION_LINES 0 jsoncons_VMAJ_LN)
        list(GET jsoncons_VERSION_LINES 1 jsoncons_VMIN_LN)
        list(GET jsoncons_VERSION_LINES 2 jsoncons_VREV_LN)
        string(REGEX MATCH "[0-9]" jsoncons_VMAJ ${jsoncons_VMAJ_LN})
        string(REGEX MATCH "[0-9]" jsoncons_VMIN ${jsoncons_VMIN_LN})
        string(REGEX MATCH "[0-9]" jsoncons_VREV ${jsoncons_VREV_LN})
        set(jsoncons_VERSION
            "${jsoncons_VMAJ}.${jsoncons_VMIN}.${jsoncons_VREV}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(jsoncons
    FOUND_VAR jsoncons_FOUND
    REQUIRED_VARS jsoncons_INCLUDE_DIR
    VERSION_VAR jsoncons_VERSION
)

if(jsoncons_FOUND)
    set(jsoncons_INCLUDE_DIRS ${jsoncons_INCLUDE_DIR})
    if(NOT TARGET jsoncons)
        add_library(jsoncons INTERFACE IMPORTED)
        target_include_directories(jsoncons SYSTEM
            INTERFACE "${jsoncons_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(jsoncons_INCLUDE_DIR)
