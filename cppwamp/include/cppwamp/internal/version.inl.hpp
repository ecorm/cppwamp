/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../version.hpp"

#include "../api.hpp"
#include "../config.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline std::string makePlatformName()
{
#ifdef CPPWAMP_CUSTOM_PLATFORM_NAME
    return CPPWAMP_CUSTOM_PLATFORM_NAME;
#else
    std::string name{CPPWAMP_SYSTEM_NAME};
#   if !defined(CPPWAMP_ARCH_IS_UNDETECTED) || defined(CPPWAMP_CUSTOM_ARCH_NAME)
    name += ' ';
    name += CPPWAMP_ARCH_NAME;
#   endif
    return name;
#endif // CPPWAMP_CUSTOM_PLATFORM_NAME
}

} // namespace internal

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
CPPWAMP_INLINE const std::string& Version::asString()
{
    static const auto str = std::to_string(CPPWAMP_MAJOR_VERSION) + '.' +
                            std::to_string(CPPWAMP_MINOR_VERSION) + '.' +
                            std::to_string(CPPWAMP_PATCH_VERSION);
    return str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Version::system()
{
    static const std::string str = CPPWAMP_SYSTEM_NAME;
    return str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Version::architecture()
{
    static const std::string str = CPPWAMP_ARCH_NAME;
    return str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Version::platform()
{
    static const auto str = internal::makePlatformName();
    return str;
}

//------------------------------------------------------------------------------
/** @details
The client agent string is formatted as:
```
cppwamp/MAJOR.MINOR.PATCH (<platform>)
```
without any zero padding of the numbers. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Version::clientAgentString()
{
    static const auto str =
        std::string{"cppwamp/"} + Version::asString() +
        " (" + internal::makePlatformName() +  ')';
    return str;
}

//------------------------------------------------------------------------------
/** @details
The server agent string kept minimal as `"cppwamp"` for security purposes.
The server agent string may be configured via:
    - ServerOptions::withAgent (for use in WAMP WELCOME messages)
    - HttpEndpoint::withAgent (for use in HTTP header 'Server' fields)
    - WebsocketEndpoint::withAgent (for use in HTTP header 'Server' fields) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& Version::serverAgentString()
{
    static const std::string str{"cppwamp"};
    return str;
}

} // namespace wamp
