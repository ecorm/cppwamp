#-------------------------------------------------------------------------------
#             Copyright Butterfly Energy Systems 2014-2015, 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

set(SOURCES
    codectestcbor.cpp
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

add_executable(cppwamp-test ${SOURCES} ${HEADERS})

# Use header-only version of CppWAMP to facilitate debugging tests.
target_link_libraries(cppwamp-test
    PRIVATE
        "$<TARGET_NAME_IF_EXISTS:Catch2::Catch2>"
        "$<TARGET_NAME_IF_EXISTS:jsoncons>"
        CppWAMP::coro-headers)
target_compile_options(cppwamp-test PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
target_compile_definitions(cppwamp-test PRIVATE
    "$<$<TARGET_EXISTS:CppWAMP::coro-headers>:CPPWAMP_TEST_HAS_CORO=1>")

# Copy Crossbar node configuration to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/.crossbar/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/.crossbar/config.json
    COPYONLY)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/.crossbar/oldconfig.json
    ${CMAKE_CURRENT_BINARY_DIR}/.crossbar/oldconfig.json
    COPYONLY)
