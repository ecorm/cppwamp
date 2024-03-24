#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2014-2015, 2022-2024.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

add_subdirectory(common)

add_subdirectory(asynctimeclient)
add_subdirectory(asynctimeservice)
add_subdirectory(futuretimeclient)
add_subdirectory(futuretimeservice)
add_subdirectory(router)

# Convenience target that groups all examples
add_custom_target(cppwamp_examples)
add_dependencies(cppwamp_examples
                 cppwamp-example-asynctimeclient
                 cppwamp-example-asynctimeservice
                 cppwamp-example-futuretimeclient
                 cppwamp-example-futuretimeservice
                 cppwamp-example-router)

if(CPPWAMP_OPT_WITH_CORO)
    add_subdirectory(chat)
    add_subdirectory(timeclient)
    add_subdirectory(timeclientauth)
    add_subdirectory(timeservice)
    add_subdirectory(stacklesstimeclient)
    add_subdirectory(stacklesstimeservice)
    add_subdirectory(streaming)
    add_dependencies(cppwamp_examples
                     cppwamp-example-chat
                     cppwamp-example-timeclient
                     cppwamp-example-timeclientauth
                     cppwamp-example-timeservice
                     cppwamp-example-stacklesstimeclient
                     cppwamp-example-stacklesstimeservice
                     cppwamp-example-streaming)
endif()

if(CPPWAMP_OPT_WITH_CORO20)
    add_subdirectory(coro20timeclient)
    add_subdirectory(coro20timeservice)
    add_dependencies(cppwamp_examples
                     cppwamp-example-coro20timeclient
                     cppwamp-example-coro20timeservice)
endif()

if(CPPWAMP_OPT_WITH_WEB)
    add_subdirectory(httpserver)
    add_subdirectory(wsrouter)
    add_subdirectory(wstimeclient)
    add_dependencies(cppwamp_examples
                     cppwamp-example-httpserver
                     cppwamp-example-wsrouter
                     cppwamp-example-wstimeclient)
endif()

if(CPPWAMP_OPT_WITH_TLS)
    add_subdirectory(tlsrouter)
    add_subdirectory(tlstimeclient)
    add_dependencies(cppwamp_examples
                     cppwamp-example-tlsrouter
                     cppwamp-example-tlstimeclient)
    if(CPPWAMP_OPT_WITH_WEB)
        add_subdirectory(httpsserver)
        add_subdirectory(wssrouter)
        add_subdirectory(wsstimeclient)
        add_dependencies(cppwamp_examples
                         cppwamp-example-httpsserver
                         cppwamp-example-wssrouter
                         cppwamp-example-wstimeclient)
    endif()
endif()

# Copy Crossbar node configuration to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/.crossbar/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/.crossbar/config.json
    COPYONLY)
