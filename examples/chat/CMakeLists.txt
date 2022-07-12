#-------------------------------------------------------------------------------
#             Copyright Butterfly Energy Systems 2014-2015, 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

add_executable(cppwamp-example-chat main.cpp )

target_link_libraries(cppwamp-example-chat
                      PRIVATE CppWAMP::core CppWAMP::coro-headers)
