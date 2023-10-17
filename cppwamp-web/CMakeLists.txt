#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2023.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

set(SOURCES
    src/cppwamp-web.cpp
)

# Minimalist target for header-only use
add_library(cppwamp-web-headers INTERFACE)
target_compile_features(cppwamp-web-headers INTERFACE cxx_std_11)
target_link_libraries(cppwamp-web-headers
    INTERFACE "$<TARGET_NAME_IF_EXISTS:cppwamp::core>"
              "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
              "$<TARGET_NAME_IF_EXISTS:Boost::filesystem>"
              "$<TARGET_NAME_IF_EXISTS:Boost::system>"
              "$<TARGET_NAME_IF_EXISTS:Threads::Threads>")
set_target_properties(cppwamp-web-headers PROPERTIES EXPORT_NAME web)
add_library(CppWAMP::web-headers ALIAS cppwamp-web-headers)


# Websocket compiled library target
if(CPPWAMP_OPT_HEADERS_ONLY)
    # Add dummy custom target so that sources are at least listed in IDEs.
    add_custom_target(cppwamp-web-sources
                      SOURCES ${SOURCES})
else()
    if(NOT APPLE)
        set(CMAKE_INSTALL_RPATH $ORIGIN)
    endif()

    add_library(cppwamp-web ${SOURCES})
    target_compile_definitions(cppwamp-web
        PUBLIC
            CPPWAMP_COMPILED_LIB=1
            "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:CPPWAMP_IS_STATIC>")
    target_compile_features(cppwamp-web PUBLIC cxx_std_11)
    target_compile_options(cppwamp-web PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

    target_include_directories(cppwamp-web
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cppwamp/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../cppwamp/include>)

    target_link_libraries(cppwamp-web
        PUBLIC
            "$<TARGET_NAME_IF_EXISTS:cppwamp::core>"
            "$<TARGET_NAME_IF_EXISTS:Boost::headers>"
            "$<TARGET_NAME_IF_EXISTS:Boost::filesystem>"
            "$<TARGET_NAME_IF_EXISTS:Boost::system>"
            "$<TARGET_NAME_IF_EXISTS:Threads::Threads>")

    set_target_properties(cppwamp-web PROPERTIES
        EXPORT_NAME web
        VERSION ${CppWAMP_VERSION}
        SOVERSION ${CppWAMP_VERSION_MAJOR})

    add_library(CppWAMP::web ALIAS cppwamp-web)
endif()
