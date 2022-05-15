/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

// The following hack is needed to fix an infinite loop issue related
// to std::exception_ptr. See http://github.com/philsquared/Catch/issues/352

#include <exception>
namespace std
{
    inline bool uncaught_exception() noexcept(true)
        {return current_exception() != nullptr;}
}

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
