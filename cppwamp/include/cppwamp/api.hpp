/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_API_HPP
#define CPPWAMP_API_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Defines macros related to exporting/importing APIs. */
//------------------------------------------------------------------------------

#ifdef CPPWAMP_COMPILED_LIB
#   define CPPWAMP_INLINE
#   if defined _WIN32 || defined __CYGWIN__
#       define CPPWAMP_API_IMPORT __declspec(dllimport)
#       define CPPWAMP_API_EXPORT __declspec(dllexport)
#       define CPPWAMP_API_HIDDEN
#   else
#       define CPPWAMP_API_IMPORT __attribute__((visibility("default")))
#       define CPPWAMP_API_EXPORT __attribute__((visibility("default")))
#       define CPPWAMP_API_HIDDEN __attribute__((visibility("hidden")))
#   endif
#   ifdef CPPWAMP_IS_STATIC
#       define CPPWAMP_API
#       define CPPWAMP_HIDDEN
#   else
#       ifdef cppwamp_core_EXPORTS // We are building this library
#           define CPPWAMP_API CPPWAMP_API_EXPORT
#       else // We are using this library
#           define CPPWAMP_API CPPWAMP_API_IMPORT
#       endif
#       define CPPWAMP_HIDDEN CPPWAMP_API_HIDDEN
#   endif
#else
#   define CPPWAMP_INLINE inline
#   define CPPWAMP_API
#   define CPPWAMP_HIDDEN
#endif

#endif // CPPWAMP_JSON_API_HPP
