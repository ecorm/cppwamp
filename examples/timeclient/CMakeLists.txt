#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

# Add executable target for chat example program
add_executable (timeclient main.cpp)

# Link against Boost libraries
target_link_libraries(timeclient ${Boost_LIBRARIES})

# Add include path for CppWAMP
include_directories(timeclient ../../cppwamp/include)
