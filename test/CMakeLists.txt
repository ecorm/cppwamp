#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

set(SOURCES
    codectestjson.cpp
    codectestmsgpack.cpp
    payloadtest.cpp
    transporttest.cpp
    varianttestassign.cpp
    varianttestbadaccess.cpp
    varianttestcomparison.cpp
    varianttestconvert.cpp
    varianttestconverter.cpp
    varianttestconvertboost.cpp
    varianttestconvertcontainers.cpp
    varianttestconverttuple.cpp
    varianttestinfo.cpp
    varianttestinit.cpp
    varianttestmap.cpp
    varianttestoutput.cpp
    varianttestvector.cpp
    varianttestvisitation.cpp
    wamptest.cpp
    wamptestadvanced.cpp
    main.cpp
)

set(HEADERS
    faketransport.hpp
    transporttest.hpp
)

# Add executable target for unit testing
add_executable (test ${SOURCES} ${HEADERS})

# Add required proprocessor macro definitions
add_definitions(
    -DCPPWAMP_COMPILED_LIB
    -DCPPWAMP_UNIT_TESTING
    -DCPPWAMP_TESTING_VARIANT=1
    -DCPPWAMP_TESTING_CODEC=1
    -DCPPWAMP_TESTING_TRANSPORT=1
    -DCPPWAMP_TESTING_WAMP=1
)

# Add include path for Catch unit test framework
include_directories(${PATH_INCLUDE_CATCH})

# Link against cppwamp and Boost libraries
target_link_libraries(test cppwamp ${Boost_LIBRARIES})

# Copy Crossbar node configuration to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/.crossbar/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/.crossbar/config.json
    COPYONLY
)
