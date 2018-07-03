#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

cmake_minimum_required (VERSION 2.8)
project (cppwamp)

# First attempt to find Boost library directories
set(BOOST_MIN_VERSION 1.58.0)
set(BOOST_COMPONENTS coroutine context thread system)
set(BOOST_ROOT ./ext/boost)
find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS ${BOOST_COMPONENTS})
if(Boost_FOUND)
    set(SUGGESTED_BOOST_INCLUDE_PATH ${Boost_INCLUDE_DIRS})
    set(SUGGESTED_BOOST_LIBRARY_PATH ${Boost_LIBRARY_DIRS})
else()
    set(SUGGESTED_BOOST_INCLUDE_PATH ./ext/boost)
    set(SUGGESTED_BOOST_LIBRARY_PATH ./ext/boost/stage/lib)
endif()

# Add GUI variables that let the user specify where the Boost directories
# are, using the above initial search for default locations.
set(PATH_INCLUDE_BOOST ${SUGGESTED_BOOST_INCLUDE_PATH} CACHE PATH
    "Boost include path")
set(PATH_LIB_BOOST ${SUGGESTED_BOOST_LIBRARY_PATH} CACHE PATH
    "Boost library path")

# Add GUI variables that let the user specify the include path for other
# third-party libraries.
set(PATH_INCLUDE_RAPIDJSON ${PROJECT_SOURCE_DIR}/ext/rapidjson/include CACHE PATH
    "RapidJson include path")
set(PATH_INCLUDE_MSGPACK ${PROJECT_SOURCE_DIR}/ext/msgpack-c/include CACHE PATH
    "Msgpack-c include path")
set(PATH_INCLUDE_CATCH ${PROJECT_SOURCE_DIR}/ext/Catch/include CACHE PATH
    "Catch include path")

# Confirm that the user's choices for the Boost library paths are valid.
unset(BOOST_ROOT)
set(BOOST_INCLUDEDIR ${PATH_INCLUDE_BOOST})
set(BOOST_LIBRARYDIR ${PATH_LIB_BOOST})
set(Boost_NO_SYSTEM_PATHS ON)
find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS ${BOOST_COMPONENTS})

# Set a default build type if none was specified.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
        "MinSizeRel" "RelWithDebInfo")
endif()

# Add the include paths to third-party libraries
include_directories(
    ${PATH_INCLUDE_BOOST}
    ${PATH_INCLUDE_RAPIDJSON}
    ${PATH_INCLUDE_MSGPACK}
)

# Add the compiler flag for C++11
if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# Add the -pthread compiler flag, if applicable.
# Note that ${Boost_LIBRARIES} already includes a linker flag for -lpthread.
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# Add the appropriate preprocessor macro define if the user specified
# non-handshaking transports.
if(CPPWAMP_USE_NON_HANDSHAKING_TRANSPORTS)
    add_definitions(-DCPPWAMP_USE_LEGACY_CONNECTORS)
endif()

# Disable Boost.Coroutine deprecation warning
add_definitions(-DBOOST_COROUTINE_NO_DEPRECATION_WARNING)
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

# Add targets for the cppwamp library itself, units tests, and examples.
add_subdirectory(cppwamp)
add_subdirectory(test)
add_subdirectory(examples)
