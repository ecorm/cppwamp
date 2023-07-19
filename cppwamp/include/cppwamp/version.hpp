/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VERSION_HPP
#define CPPWAMP_VERSION_HPP

#include <string>
#include "api.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains version information on the CppWAMP library. */
//------------------------------------------------------------------------------

// NOLINTBEGIN(modernize-macro-to-enum)

/// Major version with incompatible API changes
#define CPPWAMP_MAJOR_VERSION 0

/// Minor version with functionality added in a backwards-compatible manner.
#define CPPWAMP_MINOR_VERSION 11

/// Patch version for backwards-compatible bug fixes.
#define CPPWAMP_PATCH_VERSION 1

/// Integer version number, computed as `(major*10000) + (minor*100) + patch`
#define CPPWAMP_VERSION \
    (CPPWAMP_MAJOR_VERSION * 10000 + \
     CPPWAMP_MINOR_VERSION * 100 + \
     CPPWAMP_PATCH_VERSION)

// NOLINTEND(modernize-macro-to-enum)

namespace wamp
{

//------------------------------------------------------------------------------
/** Bundles the major, minor, and patch version numbers.
    @see CPPWAMP_MAJOR_VERSION
    @see CPPWAMP_MINOR_VERSION
    @see CPPWAMP_PATCH_VERSION
    @see CPPWAMP_VERSION */
//------------------------------------------------------------------------------
struct CPPWAMP_API Version
{
    /// Major version with incompatible API changes
    int major;

    /// Minor version with functionality added in a backwards-compatible manner.
    int minor;

    /// Patch version for backwards-compatible bug fixes.
    int patch;

    /** Obtains the current version of the library as
        major/minor/patch parts. */
    static Version parts();

    /** Obtains an integer representation of the library's current version. */
    static int integer();

    /** Obtains the library's current version as a string. */
    static std::string toString();

    /** Obtains the agent string sent in `HELLO` messages. */
    static std::string agentString();
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/version.inl.hpp"
#endif

#endif // CPPWAMP_VERSION_HPP
