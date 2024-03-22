#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2023-2024.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

add_executable(cppwamp-example-httpserver main.cpp)

target_link_libraries(cppwamp-example-httpserver
                      PRIVATE CppWAMP::web)

install(DIRECTORY www/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/www/)
install(DIRECTORY www-alt/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/www-alt/)
