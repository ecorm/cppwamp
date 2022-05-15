#-------------------------------------------------------------------------------
#           Copyright Butterfly Energy Systems 2014-2015, 2018, 2022.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

# Inspired by https://github.com/alexreinking/SharedStaticStarter

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CPPWAMP_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/CppWAMP")

if(BUILD_SHARED_LIBS)
    set(linkage shared)
else()
    set(linkage static)
endif()

install(TARGETS cppwamp-core-headers EXPORT CppWAMP_Headers
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT CppWAMP_Headers
        DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
        NAMESPACE CppWAMP::
        FILE cppwamp-core-headers-target.cmake
        COMPONENT CppWAMP_Development)

if(NOT CPPWAMP_OPT_HEADERS_ONLY)
    install(TARGETS cppwamp-core EXPORT CppWAMP_Core_Target
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                    COMPONENT CppWAMP_Runtime
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    COMPONENT CppWAMP_Runtime
                    NAMELINK_COMPONENT CppWAMP_Development
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    COMPONENT CppWAMP_Development
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(EXPORT CppWAMP_Core_Target
            DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
            NAMESPACE CppWAMP::
            FILE cppwamp-${linkage}-core-target.cmake
            COMPONENT CppWAMP_Development)
endif()

if(CPPWAMP_OPT_WITH_JSON)
    install(TARGETS cppwamp-json-headers EXPORT CppWAMP_JSON_Headers
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(EXPORT CppWAMP_JSON_Headers
            DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
            NAMESPACE CppWAMP::
            FILE cppwamp-json-headers-target.cmake
            COMPONENT CppWAMP_Development)

    if(NOT CPPWAMP_OPT_HEADERS_ONLY)
        install(TARGETS cppwamp-json EXPORT CppWAMP_JSON_Target
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                        COMPONENT CppWAMP_Runtime
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                        COMPONENT CppWAMP_Runtime
                        NAMELINK_COMPONENT CppWAMP_Development
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                        COMPONENT CppWAMP_Development
                INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

        install(EXPORT CppWAMP_JSON_Target
                DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
                NAMESPACE CppWAMP::
                FILE cppwamp-${linkage}-json-target.cmake
                COMPONENT CppWAMP_Development)
        endif()
endif()

if(CPPWAMP_OPT_WITH_MSGPACK)
    install(TARGETS cppwamp-msgpack-headers EXPORT CppWAMP_MSGPACK_Headers
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(EXPORT CppWAMP_MSGPACK_Headers
            DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
            NAMESPACE CppWAMP::
            FILE cppwamp-msgpack-headers-target.cmake
            COMPONENT CppWAMP_Development)

    if(NOT CPPWAMP_OPT_HEADERS_ONLY)
        install(TARGETS cppwamp-msgpack EXPORT CppWAMP_Msgpack_Target
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                        COMPONENT CppWAMP_Runtime
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                        COMPONENT CppWAMP_Runtime
                        NAMELINK_COMPONENT CppWAMP_Development
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                        COMPONENT CppWAMP_Development
                INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

        install(EXPORT CppWAMP_Msgpack_Target
                DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
                NAMESPACE CppWAMP::
                FILE cppwamp-${linkage}-msgpack-target.cmake
                COMPONENT CppWAMP_Development)
    endif()
endif()

if(CPPWAMP_OPT_WITH_CORO)
    install(TARGETS cppwamp-coro-headers EXPORT CppWAMP_Coro_Headers
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(EXPORT CppWAMP_Coro_Headers
            DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
            NAMESPACE CppWAMP::
            FILE cppwamp-coro-headers-target.cmake
            COMPONENT CppWAMP_Development)
endif()

# Hand-written export headers are already bundled in
# ${CppWAMP_SOURCE_DIR}/cppwamp/include/, so there are no generated
# header files in ${CppWAMP_BINARY_DIR} to install.
install(DIRECTORY "${CppWAMP_SOURCE_DIR}/cppwamp/include/"
        TYPE INCLUDE
        COMPONENT CppWAMP_Development)

write_basic_package_version_file(
    CppWAMPConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/CppWAMPConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CppWAMPConfigVersion.cmake"
        DESTINATION "${CPPWAMP_INSTALL_CMAKEDIR}"
        COMPONENT CppWAMP_Development)
