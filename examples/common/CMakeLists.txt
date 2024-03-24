#-------------------------------------------------------------------------------
# Copyright Butterfly Energy Systems 2024.
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#-------------------------------------------------------------------------------

set(HEADERS
    argsparser.hpp
    callbacktimeclient.hpp
    directtimeservice.hpp
    examplerouter.hpp
    sslclient.hpp
    sslserver.hpp
    tmconversion.hpp
)

# Add dummy custom target so that common headers are listed in IDEs.
add_custom_target(cppwamp-example-common
                  SOURCES ${HEADERS})
