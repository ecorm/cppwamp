#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2022-2023.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

include_guard()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF CACHE INTERNAL "")

# These minumum dependency versions must be made the same in CppWAMPConfig.cmake
set(CPPWAMP_MINIMUM_BOOST_VERSION 1.81.0)
set(CPPWAMP_MINIMUM_JSONCONS_VERSION 0.170.1)

set(CPPWAMP_VENDORIZED_BOOST_VERSION 1.84.0)
set(CPPWAMP_VENDORIZED_BOOST_SHA256
    "4d27e9efed0f6f152dc28db6430b9d3dfb40c0345da7342eaa5a987dde57bd95")

string(CONCAT CPPWAMP_BOOST_URL
    "https://github.com/boostorg/boost/releases/download/"
    "boost-${CPPWAMP_VENDORIZED_BOOST_VERSION}/boost-${CPPWAMP_VENDORIZED_BOOST_VERSION}.tar.gz")

if(CPPWAMP_OPT_WITH_WEB)
    list(APPEND CPPWAMP_BOOST_COMPONENTS beast url filesystem)
endif()
if(CPPWAMP_OPT_WITH_CORO)
    list(APPEND CPPWAMP_BOOST_COMPONENTS coroutine context thread)
endif()
list(APPEND CPPWAMP_BOOST_COMPONENTS asio system)

set(CPPWAMP_VENDORIZED_JSONCONS_VERSION 0.170.2)
set(CPPWAMP_VENDORIZED_JSONCONS_GIT_TAG
    "v${CPPWAMP_VENDORIZED_JSONCONS_VERSION}")

set(CPPWAMP_VENDORIZED_OPENSSL_VERSION 3.2.1)
set(CPPWAMP_VENDORIZED_OPENSSL_GIT_TAG
    "openssl-${CPPWAMP_VENDORIZED_OPENSSL_VERSION}")

set(CPPWAMP_MINIMUM_CATCH2_VERSION "2.3.0")
set(CPPWAMP_VENDORIZED_CATCH2_VERSION "2.13.10")
set(CPPWAMP_VENDORIZED_CATCH2_GIT_TAG "v${CPPWAMP_VENDORIZED_CATCH2_VERSION}")

set(CPPWAMP_BOOST_VENDOR_DIR ${PROJECT_SOURCE_DIR}/_stage/boost)
set(CPPWAMP_JSONCONS_VENDOR_DIR ${PROJECT_SOURCE_DIR}/_stage/jsoncons)
set(CPPWAMP_OPENSSL_VENDOR_DIR ${PROJECT_SOURCE_DIR}/_stage/openssl)
set(CPPWAMP_CATCH2_VENDOR_DIR ${PROJECT_SOURCE_DIR}/_stage/catch2)

#-------------------------------------------------------------------------------
# Builds an already downloaded CMake project.
#-------------------------------------------------------------------------------
function(cppwamp_build_dependency source_dir build_dir install_dir compile options)

    execute_process(
        COMMAND ${CMAKE_COMMAND} "-S${source_dir}" "-B${build_dir}"
                "-DCMAKE_INSTALL_PREFIX=${install_dir}" "${options}"
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${source_dir}
    )

    if(compile)
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build ${build_dir}
            COMMAND_ECHO STDOUT
            WORKING_DIRECTORY ${source_dir}
        )
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} --install ${build_dir}
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${source_dir}
    )

endfunction()

#-------------------------------------------------------------------------------
# Finds the vendorized Boost package
#-------------------------------------------------------------------------------
macro(cppwamp_find_vendorized_boost)

    if(BUILD_SHARED_LIBS)
        set(Boost_USE_STATIC_LIBS OFF)
    else()
        set(Boost_USE_STATIC_LIBS ON)
    endif()

    set(Boost_ROOT "${CPPWAMP_BOOST_VENDOR_DIR}")
    set(Boost_NO_SYSTEM_PATHS "${CPPWAMP_BOOST_VENDOR_DIR}")

    find_package(Boost
        ${CPPWAMP_VENDORIZED_BOOST_VERSION} EXACT
        COMPONENTS ${CPPWAMP_BOOST_COMPONENTS}
    )

endmacro()

#-------------------------------------------------------------------------------
# Finds or fetches/builds the Boost dependency.
#-------------------------------------------------------------------------------
macro(cppwamp_resolve_boost_dependency)

    if(CPPWAMP_OPT_VENDORIZE)
        cppwamp_find_vendorized_boost()
        if(NOT Boost_FOUND)
            FetchContent_Declare(
                    fetchboost
                    URL ${CPPWAMP_BOOST_URL}
                    URL_HASH SHA256=${CPPWAMP_VENDORIZED_BOOST_SHA256}
                    DOWNLOAD_EXTRACT_TIMESTAMP ON)
            FetchContent_GetProperties(fetchboost)
            if(NOT fetchboost_POPULATED)
                FetchContent_Populate(fetchboost)
            endif()
            message("CppWAMP is building Boost...")
            list(APPEND CPPWAMP_BOOST_OPTIONS
                 "-DBOOST_INCLUDE_LIBRARIES=${CPPWAMP_BOOST_COMPONENTS}")
            cppwamp_build_dependency(${fetchboost_SOURCE_DIR}
                                     ${fetchboost_BINARY_DIR}
                                     ${CPPWAMP_BOOST_VENDOR_DIR}
                                     ON
                                     "${CPPWAMP_BOOST_OPTIONS}")
            cppwamp_find_vendorized_boost()
        endif()
    else()
        # Bypass find_package if a parent project has already imported boost
        if(NOT TARGET Boost::system)
            if(BUILD_SHARED_LIBS)
                set(Boost_USE_STATIC_LIBS OFF)
            else()
                set(Boost_USE_STATIC_LIBS ON)
            endif()
            find_package(Boost
                ${CPPWAMP_MINIMUM_BOOST_VERSION}
                COMPONENTS ${CPPWAMP_BOOST_COMPONENTS})
            if(NOT ${Boost_FOUND})
                message(WARNING
"Cannot find Boost libraries. Please either define Boost_ROOT or \
enable CPPWAMP_OPT_VENDORIZE")
            endif()
        endif()
    endif()
endmacro()

#-------------------------------------------------------------------------------
# Finds the vendorized jsoncons package
#-------------------------------------------------------------------------------
macro(cppwamp_find_vendorized_jsoncons)

    find_package(jsoncons ${CPPWAMP_MINIMUM_JSONCONS_VERSION}
        CONFIG
        PATHS ${CPPWAMP_JSONCONS_VENDOR_DIR}
        NO_DEFAULT_PATH)

endmacro()

#-------------------------------------------------------------------------------
# Finds or fetches the jsoncons dependency.
#-------------------------------------------------------------------------------
macro(cppwamp_resolve_jsoncons_dependency)

    if(CPPWAMP_OPT_VENDORIZE)
        cppwamp_find_vendorized_jsoncons()
        if(NOT jsoncons_FOUND)
            FetchContent_Declare(
                    fetchjsoncons
                    GIT_REPOSITORY "https://github.com/danielaparker/jsoncons"
                    GIT_TAG ${CPPWAMP_VENDORIZED_JSONCONS_GIT_TAG}
                    GIT_SHALLOW ON)
            FetchContent_GetProperties(fetchjsoncons)
            if(NOT fetchjsoncons_POPULATED)
                FetchContent_Populate(fetchjsoncons)
            endif()
            message("CppWAMP is building jsoncons...")
            list(APPEND CPPWAMP_JSONCONS_OPTIONS
                 -DJSONCONS_BUILD_TESTS=Off)
            cppwamp_build_dependency(${fetchjsoncons_SOURCE_DIR}
                                     ${fetchjsoncons_BINARY_DIR}
                                     ${CPPWAMP_JSONCONS_VENDOR_DIR}
                                     OFF
                                     "${CPPWAMP_JSONCONS_OPTIONS}")
            cppwamp_find_vendorized_jsoncons()
        endif()
    else()
        # Bypass find_package if a parent project has already imported jsoncons
        if(NOT TARGET jsoncons)
            find_package(jsoncons ${CPPWAMP_MINIMUM_JSONCONS_VERSION})
            if(NOT ${jsoncons_FOUND})
                message(WARNING
"Cannot find jsoncons headers. Please either define jsoncons_ROOT or \
enable CPPWAMP_OPT_VENDORIZE")
            endif()
        endif()
    endif()

    # A third-party jsoncons module may not import any interface
    # library targets. Attempt to fix that here.
    if(NOT TARGET jsoncons
       AND NOT "${jsoncons_INCLUDE_DIRS}" STREQUAL ""
       AND NOT "${jsoncons_INCLUDE_DIRS}" STREQUAL
               "jsoncons_INCLUDE_DIR-NOTFOUND")
        add_library(jsoncons INTERFACE IMPORTED)
        set_target_properties(jsoncons PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${jsoncons_INCLUDE_DIRS}")
    endif()

endmacro()


#-------------------------------------------------------------------------------
# Finds the vendorized OpenSSL package
#-------------------------------------------------------------------------------
macro(cppwamp_find_vendorized_openssl)

    # These need to be unset, otherwise it will keep finding
    # the system's OpenSSL libraries first.
    unset(OPENSSL_INCLUDE_DIR CACHE)
    unset(OPENSSL_CRYPTO_LIBRARY CACHE)
    unset(OPENSSL_SSL_LIBRARY CACHE)

    if(BUILD_SHARED_LIBS)
       set(OPENSSL_USE_STATIC_LIBS OFF)
    else()
        set(OPENSSL_USE_STATIC_LIBS ON)
    endif()

    set(OpenSSL_ROOT "${CPPWAMP_OPENSSL_VENDOR_DIR}")
    set(OPENSSL_ROOT_DIR "${CPPWAMP_OPENSSL_VENDOR_DIR}")

    find_package(OpenSSL ${CPPWAMP_VENDORIZED_OPENSSL_VERSION} EXACT)

endmacro()

#-------------------------------------------------------------------------------
# Builds OpenSSL.
#-------------------------------------------------------------------------------
function(cppwamp_build_openssl source_dir install_dir)

    set(CPPWAMP_OPENSSL_CONFIG_OPTIONS
        "--prefix=${install_dir}" "--openssldir=${install_dir}" "--libdir=lib"
        "no-afalgeng" "no-async" "no-capieng" "no-cmp" "no-cms" "no-comp"
        "no-ct" "no-docs" "no-dgram" "no-dso" "no-dynamic-engine"
        "no-engine" "no-filenames" "no-gost" "no-http" "no-legacy" "no-module"
        "no-nextprotoneg" "no-ocsp" "no-padlockeng" "no-sock" "no-srp"
        "no-srtp" "no-ssl-trace" "no-static-engine" "no-quic"
        "no-ts" "no-ui-console" "no-uplink"
        "no-ssl3-method" "no-tls1-method" "no-tls1_1-method" "no-dtls1-method"
        "no-dtls1_2-method"
        "no-argon2" "no-bf" "no-blake2" "no-cast" "no-cmac" "no-des" "no-dsa"
        "no-idea" "no-md4" "no-mdc2" "no-ocb" "no-rc2" "no-rc4" "no-rmd160"
        "no-scrypt" "no-siphash" "no-siv" "no-sm2" "no-sm3" "no-sm4"
        "no-whirlpool"
        # "no-tests"
    )

    if(OPENSSL_USE_STATIC_LIBS)
        list(APPEND CPPWAMP_OPENSSL_CONFIG_OPTIONS "no-shared")
    endif()

    execute_process(
        COMMAND "./Configure" ${CPPWAMP_OPENSSL_CONFIG_OPTIONS}
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${source_dir}
    )

    execute_process(
        COMMAND "make" "-j${CPPWAMP_CORE_COUNT}"
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${source_dir}
    )

    execute_process(
        COMMAND "make" "install"
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${source_dir}
    )

endfunction()

#-------------------------------------------------------------------------------
# Finds or fetches/builds the OpenSSL dependency.
#-------------------------------------------------------------------------------
macro(cppwamp_resolve_openssl_dependency)

    if(CPPWAMP_OPT_VENDORIZE)
        cppwamp_find_vendorized_openssl()
        if(NOT OPENSSL_FOUND)
            FetchContent_Declare(
                    fetchopenssl
                    GIT_REPOSITORY "https://github.com/openssl/openssl.git"
                    GIT_TAG ${CPPWAMP_VENDORIZED_OPENSSL_GIT_TAG}
                    GIT_SUBMODULES ""
                    GIT_SHALLOW ON)
            FetchContent_GetProperties(fetchopenssl)
            if(NOT fetchopenssl_POPULATED)
                FetchContent_Populate(fetchopenssl)
            endif()
            message("CppWAMP is building OpenSSL...")
            cppwamp_build_openssl(${fetchopenssl_SOURCE_DIR}
                                  ${CPPWAMP_OPENSSL_VENDOR_DIR})
            cppwamp_find_vendorized_openssl()
        endif()
    else()
        # Bypass find_package if a parent project has already imported OpenSSL
        if(NOT TARGET OpenSSL::SSL)
            if(BUILD_SHARED_LIBS)
                set(OPENSSL_USE_STATIC_LIBS OFF)
            else()
                set(OPENSSL_USE_STATIC_LIBS ON)
            endif()
            find_package(OpenSSL)
            if(NOT ${OPENSSL_FOUND})
                message(WARNING
"Cannot find OpenSSL libraries. Please either define OPENSSL_ROOT_DIR or \
enable CPPWAMP_OPT_VENDORIZE")
            endif()
        endif()
    endif()
endmacro()


#-------------------------------------------------------------------------------
# Finds the vendorized Catch2 package
#-------------------------------------------------------------------------------
macro(cppwamp_find_vendorized_catch2)

    find_package(Catch2
        CONFIG
        PATHS ${CPPWAMP_CATCH2_VENDOR_DIR}
        NO_DEFAULT_PATH)

endmacro()

#-------------------------------------------------------------------------------
# Finds or fetches the Catch2 dependency.
#-------------------------------------------------------------------------------
macro(cppwamp_resolve_catch2_dependency)

    if(CPPWAMP_OPT_VENDORIZE)
        cppwamp_find_vendorized_catch2()
        if(NOT Catch2_FOUND)
            FetchContent_Declare(
                    fetchcatch2
                    GIT_REPOSITORY "https://github.com/catchorg/Catch2"
                    GIT_TAG ${CPPWAMP_VENDORIZED_CATCH2_GIT_TAG}
                    GIT_SHALLOW ON
            )
            FetchContent_GetProperties(fetchcatch2)
            if(NOT fetchcatch2_POPULATED)
                FetchContent_Populate(fetchcatch2)
            endif()
            message("CppWAMP is building Catch2 version
                    ${CPPWAMP_VENDORIZED_CATCH2_VERSION}...")
            list(APPEND CPPWAMP_CATCH2_OPTIONS
                 -DCATCH_BUILD_TESTING=Off
                 -DCATCH_ENABLE_WERROR=Off
                 -DCATCH_INSTALL_DOCS=Off
                 -DCATCH_INSTALL_HELPERS=Off)
            cppwamp_build_dependency(${fetchcatch2_SOURCE_DIR}
                                     ${fetchcatch2_BINARY_DIR}
                                     ${CPPWAMP_CATCH2_VENDOR_DIR}
                                     OFF
                                     "${CPPWAMP_CATCH2_OPTIONS}")
            cppwamp_find_vendorized_catch2()
        endif()
    else()
        # Bypass find_package if a parent project has already imported Catch2
        if(NOT TARGET Catch2::Catch2)
            find_package(Catch2 ${CPPWAMP_MINIMUM_CATCH2_VERSION})
            if(NOT ${Catch2_FOUND})
                message(WARNING
"Cannot find the Catch2 library. Please either define Catch2_ROOT or \
enable CPPWAMP_OPT_VENDORIZE")
            endif()
        endif()
    endif()

    # A third-party FindCatch2 module may not import any interface
    # library targets, or such targets may lack the expected namespace.
    # Attempt to fix that here.
    if(NOT TARGET Catch2::Catch2)
        if(NOT TARGET Catch2)
            if(NOT "${Catch2_INCLUDE_DIR}" STREQUAL ""
               AND NOT "${Catch2_INCLUDE_DIR}" STREQUAL
                       "Catch2_INCLUDE_DIR-NOTFOUND")
                add_library(Catch2::Catch2 INTERFACE IMPORTED)
                set_target_properties(Catch2::Catch2 PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${Catch2_INCLUDE_DIR}")
            endif()
        else()
            add_library(Catch2::Catch2 ALIAS Catch2)
        endif()
    endif()

endmacro()


#-------------------------------------------------------------------------------
# Finds or fetches/builds CppWAMP dependencies according to the
# CPPWAMP_OPT_VENDORIZE user option, as well as the various
# CPPWAMP_OPT_NO_<foo> and CPPWAMP_OPT_WITH_<foo> options.
# Macros are used here so that the output variables of find_package are
# available to the macro caller.
#-------------------------------------------------------------------------------
macro(cppwamp_resolve_dependencies)

    # Import threading support
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    # If CPPWAMP_OPT_VENDORIZE was toggled by the user, clear the cache
    # variables that were previously set by find_package calls so that
    # subsequent find_package calls actually perform the search.
    if(NOT ${CPPWAMP_PREVIOUSLY_VENDORIZED} EQUAL ${CPPWAMP_OPT_VENDORIZE})
        unset(Boost_DIR CACHE)
        unset(Boost_INCLUDE_DIR CACHE)
        unset(Boost_LIBRARY_DIR_DEBUG CACHE)
        unset(Boost_LIBRARY_DIR_RELEASE CACHE)
        unset(Boost_ATOMIC_LIBRARY_DEBUG CACHE)
        unset(Boost_ATOMIC_LIBRARY_RELEASE CACHE)
        unset(Boost_CONTEXT_LIBRARY_DEBUG CACHE)
        unset(Boost_CONTEXT_LIBRARY_RELEASE CACHE)
        unset(Boost_COROUTINE_LIBRARY_DEBUG CACHE)
        unset(Boost_COROUTINE_LIBRARY_RELEASE CACHE)
        unset(Boost_FILESYSTEM_LIBRARY_DEBUG CACHE)
        unset(Boost_FILESYSTEM_LIBRARY_RELEASE CACHE)
        unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
        unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
        unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
        unset(Boost_THREAD_LIBRARY_RELEASE CACHE)
        unset(jsoncons_INCLUDE_DIR CACHE)
        unset(Catch2_INCLUDE_DIR CACHE)
    endif()
    set(CPPWAMP_PREVIOUSLY_VENDORIZED ${CPPWAMP_OPT_VENDORIZE}
        CACHE INTERNAL "" FORCE)

    set(CMAKE_THREAD_PREFER_PTHREAD ON)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)

    cppwamp_resolve_boost_dependency()
    cppwamp_resolve_jsoncons_dependency()
    if(CPPWAMP_OPT_WITH_TLS)
        cppwamp_resolve_openssl_dependency()
    endif()
    if(CPPWAMP_OPT_WITH_TESTS)
        cppwamp_resolve_catch2_dependency()
    endif()

    message("CppWAMP using Boost from ${Boost_INCLUDE_DIRS}")

    if(TARGET jsoncons)
        get_target_property(CPPWAMP_JSONCONS_INCLUDE_DIR jsoncons
                            INTERFACE_INCLUDE_DIRECTORIES)
        message("CppWAMP using jsoncons from ${CPPWAMP_JSONCONS_INCLUDE_DIR}")
    endif()

    if(CPPWAMP_OPT_WITH_TLS AND TARGET OpenSSL::SSL)
        get_target_property(CPPWAMP_OPENSSL_INCLUDE_DIR OpenSSL::SSL
                            INTERFACE_INCLUDE_DIRECTORIES)
        message("CppWAMP using OpenSSL from ${CPPWAMP_OPENSSL_INCLUDE_DIR}")
    endif()

    if(CPPWAMP_OPT_WITH_TESTS AND TARGET Catch2::Catch2)
        get_target_property(CPPWAMP_CATCH2_INCLUDE_DIR Catch2::Catch2
                            INTERFACE_INCLUDE_DIRECTORIES)
        message("CppWAMP using Catch2 from ${CPPWAMP_CATCH2_INCLUDE_DIR}")
    endif()

    set(CPPWAMP_BOOST_LIBRARIES ${Boost_LIBRARIES})

endmacro()
