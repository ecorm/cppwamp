#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2022.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

add_executable(cppwamp-example-coro20timeclient main.cpp)

target_compile_features(cppwamp-example-coro20timeclient PRIVATE cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cppwamp-example-coro20timeclient
                           PRIVATE "-stdlib=libc++" "-fcoroutines-ts"
                                   "-ftemplate-backtrace-limit=0")
    target_link_options(cppwamp-example-coro20timeclient
                        PRIVATE "-stdlib=libc++")
    target_link_libraries(cppwamp-example-coro20timeclient
                          PRIVATE CppWAMP::core-headers
                                  "$<TARGET_NAME_IF_EXISTS:jsoncons>")
else ()
    target_compile_options(cppwamp-example-coro20timeclient
                           PRIVATE "-fcoroutines")
    target_link_libraries(cppwamp-example-coro20timeclient
                          PRIVATE CppWAMP::core)
endif ()
