/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXAMPLES_TMCONVERSION_HPP
#define CPPWAMP_EXAMPLES_TMCONVERSION_HPP

#include <ctime>
#include <cppwamp/variant.hpp>

namespace wamp
{

// Convert a std::tm to/from an object variant.
template <typename TConverter>
void convert(TConverter& conv, std::tm& t)
{
    conv("sec",   t.tm_sec)
        ("min",   t.tm_min)
        ("hour",  t.tm_hour)
        ("mday",  t.tm_mday)
        ("mon",   t.tm_mon)
        ("year",  t.tm_year)
        ("wday",  t.tm_wday)
        ("yday",  t.tm_yday)
        ("isdst", t.tm_isdst);
}

} // namespace wamp

#endif // CPPWAMP_EXAMPLES_TMCONVERSION_HPP
