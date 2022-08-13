#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2022.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

add_executable(cppwamp-example-asynctimeservice main.cpp)

target_link_libraries(cppwamp-example-asynctimeservice
                      PRIVATE CppWAMP::core)
