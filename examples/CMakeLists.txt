#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2022
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

if(CPPWAMP_OPT_WITH_JSON AND CPPWAMP_OPT_WITH_CORO)
    add_subdirectory(chat)
    add_subdirectory(timeservice)
    add_subdirectory(timeclient)

    # Convenience target that groups all examples
    add_custom_target(cppwamp_examples)
    add_dependencies(cppwamp_examples
                     cppwamp-example-chat
                     cppwamp-example-timeclient
                     cppwamp-example-timeservice)
    # Copy Crossbar node configuration to build directory
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/.crossbar/config.json
        ${CMAKE_CURRENT_BINARY_DIR}/.crossbar/config.json
        COPYONLY)
endif()
