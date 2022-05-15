#-------------------------------------------------------------------------------
#                  Copyright Butterfly Energy Systems 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

add_library(cppwamp-coro-headers INTERFACE)
target_compile_features(cppwamp-coro-headers INTERFACE cxx_std_11)
target_link_libraries(cppwamp-coro-headers
    INTERFACE
        CppWAMP::core-headers
        "$<TARGET_NAME_IF_EXISTS:Boost::coroutine>")
set_target_properties(cppwamp-coro-headers PROPERTIES EXPORT_NAME coro-headers)
add_library(CppWAMP::coro-headers ALIAS cppwamp-coro-headers)
