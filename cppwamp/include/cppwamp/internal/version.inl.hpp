/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../version.hpp"

#include <sstream>
#include "../api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Version Version::parts()
{
    return {CPPWAMP_MAJOR_VERSION,
            CPPWAMP_MINOR_VERSION,
            CPPWAMP_PATCH_VERSION};
}

//------------------------------------------------------------------------------
/** @details
The integer version number is computed as:
```
(MAJOR*10000) + (MINOR*100) + PATCH
```
*/
//------------------------------------------------------------------------------
CPPWAMP_INLINE int Version::integer()
{
    return CPPWAMP_VERSION;
}

//------------------------------------------------------------------------------
/** @details
The string representation is formatted as:
```
MAJOR.MINOR.PATCH
```
without any zero padding. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string Version::toString()
{
    std::ostringstream oss;
    oss << CPPWAMP_MAJOR_VERSION << '.'
        << CPPWAMP_MINOR_VERSION << '.'
        << CPPWAMP_PATCH_VERSION;
    return oss.str();
}

//------------------------------------------------------------------------------
/** @details
The agent string is formatted as:
```
cppwamp/MAJOR.MINOR.PATCH
```
without any zero padding. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Version::agentString()
{
    // TODO: Add platform/OS
    // TODO: Different client/server strings?
    static const std::string str{"cppwamp/" + Version::toString()};
    return str;
}

} // namespace wamp
