#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2024.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

set(SOURCES
    src/cppwamp-tls.cpp
)

# Minimalist target for header-only use
add_library(cppwamp-tls-headers INTERFACE)
target_compile_features(cppwamp-tls-headers INTERFACE cxx_std_11)
target_link_libraries(cppwamp-tls-headers
    INTERFACE "$<TARGET_NAME_IF_EXISTS:CppWAMP::core>"
              "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
              "$<TARGET_NAME_IF_EXISTS:Boost::system>"
              "$<TARGET_NAME_IF_EXISTS:OpenSSL::SSL>"
              "$<TARGET_NAME_IF_EXISTS:OpenSSL::Cryto>"
              "$<TARGET_NAME_IF_EXISTS:Threads::Threads>")
set_target_properties(cppwamp-tls-headers PROPERTIES EXPORT_NAME tls)
add_library(CppWAMP::tls-headers ALIAS cppwamp-tls-headers)


# TLS compiled library target
if(CPPWAMP_OPT_HEADERS_ONLY)
    # Add dummy custom target so that sources are at least listed in IDEs.
    add_custom_target(cppwamp-tls-sources
                      SOURCES ${SOURCES})
else()
    if(NOT APPLE)
        set(CMAKE_INSTALL_RPATH $ORIGIN)
    endif()

    add_library(cppwamp-tls ${SOURCES})
    target_compile_definitions(cppwamp-tls
        PUBLIC
            CPPWAMP_COMPILED_LIB=1
            "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:CPPWAMP_IS_STATIC>")
    target_compile_features(cppwamp-tls PUBLIC cxx_std_11)
    target_compile_options(cppwamp-tls PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

    target_include_directories(cppwamp-tls
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cppwamp/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../cppwamp/include>)

    target_link_libraries(cppwamp-tls
        PUBLIC
            "$<TARGET_NAME_IF_EXISTS:CppWAMP::core>"
            "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
            "$<TARGET_NAME_IF_EXISTS:Boost::system>"
            "$<TARGET_NAME_IF_EXISTS:OpenSSL::SSL>"
            "$<TARGET_NAME_IF_EXISTS:OpenSSL::Cryto>"
            "$<TARGET_NAME_IF_EXISTS:Threads::Threads>")

    set_target_properties(cppwamp-tls PROPERTIES
        EXPORT_NAME tls
        VERSION ${CppWAMP_VERSION}
        SOVERSION ${CppWAMP_VERSION_MAJOR})

    add_library(CppWAMP::tls ALIAS cppwamp-tls)
endif()
