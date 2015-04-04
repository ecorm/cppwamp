#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

# Add executable target for chat example program
add_executable (chat main.cpp)

# Add required preprocessor macro definitions
add_definitions(-DCPPWAMP_COMPILED_LIB)

# Link against cppwamp and Boost libraries
target_link_libraries(chat cppwamp ${Boost_LIBRARIES})

