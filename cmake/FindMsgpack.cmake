#-------------------------------------------------------------------------------
#                 Copyright Butterfly Energy Systems 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

#[=======================================================================[.rst:
FindMsgpack
-----------

Finds the msgpack C++ library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``msgpackc-cxx``
  The msgpack C++ header-only interface library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Msgpack_FOUND``
  True if the system has the msgpack C++ headers.
``Msgpack_VERSION``
  The version of the msgpack C++ library which was found.
``Msgpack_INCLUDE_DIRS``
  Include directories needed to use msgpack C++.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Msgpack_INCLUDE_DIR``
  The directory containing ``msgpack.hpp``.

Hints
^^^^^

This module reads hints about search locations from variables:

``Msgpack_ROOT``
  Preferred installation prefix.

#]=======================================================================]

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_Msgpack QUIET msgpack)
endif()

find_path(Msgpack_INCLUDE_DIR msgpack.hpp
    HINTS ${PC_Msgpack_INCLUDE_DIRS} ${PC_Msgpack_INCLUDEDIR}
)
set(Msgpack_VERSION ${PC_Msgpack_VERSION})

set(Msgpack_VERSION_FILE
    "${Msgpack_INCLUDE_DIR}/msgpack/version_master.hpp")
if(NOT "${Msgpack_INCLUDE_DIR}" STREQUAL "Msgpack_INCLUDE_DIR-NOTFOUND"
   AND "${Msgpack_VERSION}" STREQUAL ""
   AND EXISTS ${Msgpack_VERSION_FILE})
    # Expected version_master.hpp file format:
    #define MSGPACK_VERSION_MAJOR    N
    #define MSGPACK_VERSION_MINOR    N
    #define MSGPACK_VERSION_REVISION N
    file(STRINGS ${Msgpack_VERSION_FILE} Msgpack_VERSION_LINES
         REGEX "#define MSGPACK_VERSION"
         LIMIT_COUNT 3)
    list(LENGTH Msgpack_VERSION_LINES Msgpack_VERSION_LINE_COUNT)
    if(${Msgpack_VERSION_LINE_COUNT} GREATER 2)
        list(GET Msgpack_VERSION_LINES 0 Msgpack_VMAJ_LN)
        list(GET Msgpack_VERSION_LINES 1 Msgpack_VMIN_LN)
        list(GET Msgpack_VERSION_LINES 2 Msgpack_VREV_LN)
        string(REGEX MATCH "[0-9]" Msgpack_VMAJ ${Msgpack_VMAJ_LN})
        string(REGEX MATCH "[0-9]" Msgpack_VMIN ${Msgpack_VMIN_LN})
        string(REGEX MATCH "[0-9]" Msgpack_VREV ${Msgpack_VREV_LN})
        set(Msgpack_VERSION
            "${Msgpack_VMAJ}.${Msgpack_VMIN}.${Msgpack_VREV}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Msgpack
    FOUND_VAR Msgpack_FOUND
    REQUIRED_VARS Msgpack_INCLUDE_DIR
    VERSION_VAR Msgpack_VERSION
)

if(Msgpack_FOUND)
    set(Msgpack_INCLUDE_DIRS ${Msgpack_INCLUDE_DIR})
    if(NOT TARGET msgpackc-cxx)
        add_library(msgpackc-cxx INTERFACE IMPORTED)
        target_include_directories(msgpackc-cxx SYSTEM
            INTERFACE "${Msgpack_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(Msgpack_INCLUDE_DIR)
