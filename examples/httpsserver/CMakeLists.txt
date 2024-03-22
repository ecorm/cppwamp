#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2023-2024.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

add_executable(cppwamp-example-httpsserver main.cpp)

target_link_libraries(cppwamp-example-httpsserver
                      PRIVATE CppWAMP::web-tls)

install(DIRECTORY certs/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/certs/)
install(DIRECTORY www/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/www/)
install(DIRECTORY www-alt/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/www-alt/)
