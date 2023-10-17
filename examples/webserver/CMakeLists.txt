#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2023.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

add_executable(cppwamp-example-webserver main.cpp)

target_link_libraries(cppwamp-example-webserver
                      PRIVATE CppWAMP::web CppWAMP::core)

install(DIRECTORY www/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/www/)
