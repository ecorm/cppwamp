#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

# Add executable target for futucalc example program
add_executable (futucalc main.cpp)

# Add required preprocessor macro definitions
add_definitions(
    -DCPPWAMP_COMPILED_LIB
    -DBOOST_THREAD_VERSION=4
    -DBOOST_THREAD_PROVIDES_EXECUTORS
)

# Link against cppwamp and Boost libraries
target_link_libraries(futucalc cppwamp ${Boost_LIBRARIES})

