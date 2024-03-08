#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2024.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

set(SOURCES
    src/cppwamp-web-tls.cpp
)

# Minimalist target for header-only use
add_library(cppwamp-web-tls-headers INTERFACE)
target_compile_features(cppwamp-web-tls-headers INTERFACE cxx_std_11)
target_link_libraries(cppwamp-web-tls-headers
    INTERFACE "$<TARGET_NAME_IF_EXISTS:CppWAMP::core>"
              "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
              "$<TARGET_NAME_IF_EXISTS:Boost::filesystem>"
              "$<TARGET_NAME_IF_EXISTS:Boost::url>"
              "$<TARGET_NAME_IF_EXISTS:Boost::system>"
              "$<TARGET_NAME_IF_EXISTS:OpenSSL::SSL>"
              "$<TARGET_NAME_IF_EXISTS:OpenSSL::Cryto>"
              "$<TARGET_NAME_IF_EXISTS:Threads::Threads>")
set_target_properties(cppwamp-web-tls-headers PROPERTIES EXPORT_NAME web)
add_library(CppWAMP::web-tls-headers ALIAS cppwamp-web-tls-headers)


# Websocket compiled library target
if(CPPWAMP_OPT_HEADERS_ONLY)
    # Add dummy custom target so that sources are at least listed in IDEs.
    add_custom_target(cppwamp-web-tls-sources
                      SOURCES ${SOURCES})
else()
    if(NOT APPLE)
        set(CMAKE_INSTALL_RPATH $ORIGIN)
    endif()

    add_library(cppwamp-web-tls ${SOURCES})
    target_compile_definitions(cppwamp-web
        PUBLIC
            CPPWAMP_COMPILED_LIB=1
            "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:CPPWAMP_IS_STATIC>")
    target_compile_features(cppwamp-web-tls PUBLIC cxx_std_11)
    target_compile_options(cppwamp-web-tls PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

    target_include_directories(cppwamp-web-tls
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cppwamp/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../cppwamp/include>)

    target_link_libraries(cppwamp-web-tls
        PUBLIC
            "$<TARGET_NAME_IF_EXISTS:CppWAMP::core>"
            "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
            "$<TARGET_NAME_IF_EXISTS:Boost::filesystem>"
            "$<TARGET_NAME_IF_EXISTS:Boost::url>"
            "$<TARGET_NAME_IF_EXISTS:Boost::system>"
            "$<TARGET_NAME_IF_EXISTS:OpenSSL::SSL>"
            "$<TARGET_NAME_IF_EXISTS:OpenSSL::Cryto>"
            "$<TARGET_NAME_IF_EXISTS:Threads::Threads>")

    set_target_properties(cppwamp-web-tls PROPERTIES
        EXPORT_NAME web-tls
        VERSION ${CppWAMP_VERSION}
        SOVERSION ${CppWAMP_VERSION_MAJOR})

    add_library(CppWAMP::web-tls ALIAS cppwamp-web-tls)
endif()
