#-------------------------------------------------------------------------------
#                 Copyright Butterfly Energy Systems 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

#[=======================================================================[.rst:
FindCatch2
-------------

Finds the Catch2 v2 header-only library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Catch2::Catch2``
  The Catch2 v2 header-only interface library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Catch2_FOUND``
  True if the system has the Catch2 headers.
``Catch2_VERSION``
  The version of the Catch2 library which was found.
``Catch2_INCLUDE_DIRS``
  Include directories needed to use Catch2.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Catch2_INCLUDE_DIR``
  The ``single_include`` directory containing ``catch2/catch.hpp``.

Hints
^^^^^

This module reads hints about search locations from variables:

``Catch2_ROOT``
  Preferred installation prefix.

#]=======================================================================]

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_Catch2 QUIET catch2)
endif()

find_path(Catch2_INCLUDE_DIR "catch2/catch.hpp"
    HINTS ${PC_Catch2_INCLUDE_DIRS} ${PC_Catch2_INCLUDEDIR}
)
set(Catch2_VERSION ${PC_Catch2_VERSION})

set(Catch2_VERSION_FILE
    "${Catch2_INCLUDE_DIR}/catch2/catch.hpp")
if(NOT "${Catch2_INCLUDE_DIR}" STREQUAL "Catch2_INCLUDE_DIR-NOTFOUND"
   AND "${Catch2_VERSION}" STREQUAL ""
   AND EXISTS ${Catch2_VERSION_FILE})
    # Expected format:
    #define CATCH_VERSION_MAJOR N
    #define CATCH_VERSION_MINOR N
    #define CATCH_VERSION_PATCH N
    file(STRINGS ${Catch2_VERSION_FILE} Catch2_VERSION_LINES
         REGEX "#define CATCH_VERSION_"
         LIMIT_COUNT 3)
    list(LENGTH Catch2_VERSION_LINES Catch2_VERSION_LINE_COUNT)
    if(${Catch2_VERSION_LINE_COUNT} GREATER 2)
        list(GET Catch2_VERSION_LINES 0 Catch2_VMAJ_LN)
        list(GET Catch2_VERSION_LINES 1 Catch2_VMIN_LN)
        list(GET Catch2_VERSION_LINES 2 Catch2_VREV_LN)
        string(REGEX MATCH "[0-9]" Catch2_VMAJ ${Catch2_VMAJ_LN})
        string(REGEX MATCH "[0-9]" Catch2_VMIN ${Catch2_VMIN_LN})
        string(REGEX MATCH "[0-9]" Catch2_VREV ${Catch2_VREV_LN})
        set(Catch2_VERSION
            "${Catch2_VMAJ}.${Catch2_VMIN}.${Catch2_VREV}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Catch2
    FOUND_VAR Catch2_FOUND
    REQUIRED_VARS Catch2_INCLUDE_DIR
    VERSION_VAR Catch2_VERSION
)

if(Catch2_FOUND)
    set(Catch2_INCLUDE_DIRS ${Catch2_INCLUDE_DIR})

    if(NOT TARGET Catch2::Catch2)
        if(NOT TARGET Catch2)
            add_library(Catch2::Catch2 INTERFACE IMPORTED)
        else()
            add_library(Catch2::Catch2 ALIAS Catch2)
        endif()
        target_include_directories(Catch2::Catch2 SYSTEM
            INTERFACE "${Catch2_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(Catch2_INCLUDE_DIR)
