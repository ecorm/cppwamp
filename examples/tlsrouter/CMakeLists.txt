#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2024.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

add_executable(cppwamp-example-tlsrouter main.cpp)

target_link_libraries(cppwamp-example-tlsrouter
                      PRIVATE CppWAMP::tls)

# Copy SSL certificates to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/certs/dh4096.pem
    ${CMAKE_CURRENT_BINARY_DIR}/certs/dh4096.pem
    COPYONLY)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/certs/localhost.crt
    ${CMAKE_CURRENT_BINARY_DIR}/certs/localhost.crt
    COPYONLY)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/certs/localhost.key
    ${CMAKE_CURRENT_BINARY_DIR}/certs/localhost.key
    COPYONLY)
