/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include "testrouter.hpp"

//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    Catch::Session session;

    bool launchRouter = true;

    // Extend Catch's parser with our own CLI options
    using namespace Catch::clara;
    auto cli
        = session.cli() // Get Catch's composite command line parser
          | Opt(launchRouter, "yes|no")["--router"]
            ("launch CppWAMP's own test router (default: yes)");

    // Pass the extended parser back to Catch
    session.cli(cli);

    // Parse the command line
    int result = session.applyCommandLine( argc, argv );
    if (result != 0 || session.config().showHelp())
        return result;

    // Launch the router before running the tests, if enabled
    if (launchRouter)
    {
        auto& router = test::TestRouter::instance();
        router.start();
        result = session.run();
        router.stop();
    }
    else
    {
        result = session.run();
    }

    return result;
}
