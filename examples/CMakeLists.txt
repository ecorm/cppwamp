#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2014-2015, 2022.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

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
    add_dependencies(cppwamp-example-coro20timeclient
                     cppwamp-example-coro20timeservice)
endif()

# Copy Crossbar node configuration to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/.crossbar/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/.crossbar/config.json
    COPYONLY)
