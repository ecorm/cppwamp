#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2022.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

# Empty interface library for conveniently adding usage requirements to
# projects wanting to use stackful coroutines.
add_library(cppwamp-coro-usage INTERFACE)
target_compile_features(cppwamp-coro-usage INTERFACE cxx_std_11)
target_link_libraries(cppwamp-coro-usage
    INTERFACE
        CppWAMP::coro-usage
        "$<TARGET_NAME_IF_EXISTS:Boost::coroutine>")
set_target_properties(cppwamp-coro-usage PROPERTIES EXPORT_NAME coro-usage)
add_library(CppWAMP::coro-usage ALIAS cppwamp-coro-usage)

# Deprecated aliases left for backward compatibility
add_library(cppwamp-coro-headers ALIAS cppwamp-coro-usage)
add_library(CppWAMP::coro-headers ALIAS cppwamp-coro-usage)
